#include "OrbbecAdapter.h"

#include "input/sensors/DepthStore.h"

#include <nlohmann/json.hpp>

#include <ofLog.h>
#include <ofMath.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using json = nlohmann::json;

namespace modular {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
OrbbecAdapter::OrbbecAdapter() {
    currentBody_.bodyId = 1;
    currentBody_.isTracked = false;
    currentBody_.confidence = 0.0f;
    for (int i = 0; i < static_cast<int>(JointType::COUNT); ++i) {
        currentBody_.joints[i].type       = static_cast<JointType>(i);
        currentBody_.joints[i].position   = {0.5f, 0.5f, 0.5f};
        currentBody_.joints[i].velocity   = {0.0f, 0.0f, 0.0f};
        currentBody_.joints[i].confidence = 0.0f;
    }
    previousBody_ = currentBody_;
}

OrbbecAdapter::~OrbbecAdapter() {
    shutdown();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
bool OrbbecAdapter::initialize() {
    if (!setupCamera()) {
        connectionState_ = ConnectionState::ERROR;
        return false;
    }

    detectorRunning_ = true;
    detectorThread_ = std::thread(&OrbbecAdapter::detectorLoop, this);
    connectionState_ = ConnectionState::CONNECTED;
    ofLogNotice("OrbbecAdapter") << "cámara Orbbec iniciada + detector TCP en puerto " << detectorPort_;
    return true;
}

void OrbbecAdapter::shutdown() {
    detectorRunning_ = false;
    if (detectorThread_.joinable()) detectorThread_.join();

    if (pipe_) {
        try {
            pipe_->stop();
        } catch (...) {}
        pipe_.reset();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    connectionState_ = ConnectionState::DISCONNECTED;
    // Preserve callbacks across shutdown/initialize cycles (see MediaPipePoseAdapter).
}

// ---------------------------------------------------------------------------
// Per-frame update (main thread)
// ---------------------------------------------------------------------------
void OrbbecAdapter::update() {
    grabLatestFrame();

    // Read latest poses (multi-person) from the detector thread.
    std::vector<std::vector<std::array<float, 3>>> poses;
    std::vector<std::vector<float>> visList;
    {
        std::lock_guard<std::mutex> lock(poseMutex_);
        poses = latestPoses_;
        visList = latestVisList_;
    }

    bool any = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        trackedBodies_.clear();
    }
    for (size_t i = 0; i < poses.size() && i < 8; i++) {
        if (poses[i].size() < 33) continue;
        applyPose(poses[i], visList[i], static_cast<int>(i) + 1);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            trackedBodies_.push_back(currentBody_);
        }
        fireFrameCallbacks(currentBody_);
        any = true;
    }

    if (any) {
        presence_ = 1.0f;
    } else {
        // No detection: decay presence toward 0 for smooth fade.
        presence_ = ofLerp(presence_, 0.0f, 0.05f);
    }
}

ConnectionState OrbbecAdapter::getConnectionState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connectionState_;
}

// ---------------------------------------------------------------------------
// Camera setup (OrbbecSDK)
// ---------------------------------------------------------------------------
bool OrbbecAdapter::setupCamera() {
    try {
        pipe_ = std::make_shared<ob::Pipeline>();

        std::shared_ptr<ob::VideoStreamProfile> colorProfile;
        auto colorList = pipe_->getStreamProfileList(OB_SENSOR_COLOR);
        for (uint32_t i = 0; i < colorList->getCount(); i++) {
            auto vsp = colorList->getProfile(i)->as<ob::VideoStreamProfile>();
            if (vsp->getFormat() != OB_FORMAT_RGB) continue;
            if (vsp->getWidth() == 1280 && vsp->getHeight() == 720 && vsp->getFps() == 30) {
                colorProfile = vsp;
                break;
            }
        }
        if (!colorProfile) {
            for (uint32_t i = 0; i < colorList->getCount(); i++) {
                auto vsp = colorList->getProfile(i)->as<ob::VideoStreamProfile>();
                if (vsp->getFormat() == OB_FORMAT_RGB) { colorProfile = vsp; break; }
            }
        }
        if (!colorProfile) { ofLogError("OrbbecAdapter") << "sin perfil de color RGB"; return false; }

        std::shared_ptr<ob::VideoStreamProfile> depthProfile;
        auto d2cList = pipe_->getD2CDepthProfileList(colorProfile, ALIGN_D2C_HW_MODE);
        for (uint32_t i = 0; i < d2cList->getCount(); i++) {
            auto vsp = d2cList->getProfile(i)->as<ob::VideoStreamProfile>();
            if (vsp->getFormat() == OB_FORMAT_Y16) { depthProfile = vsp; break; }
        }
        if (!depthProfile) { ofLogError("OrbbecAdapter") << "sin perfil depth Y16 alineado"; return false; }

        auto config = std::make_shared<ob::Config>();
        config->enableStream(colorProfile);
        config->enableStream(depthProfile);
        config->setAlignMode(ALIGN_D2C_HW_MODE);

        pipe_->start(config, [this](std::shared_ptr<ob::FrameSet> fs) {
            std::lock_guard<std::mutex> lock(frameMutex_);
            latestFrames_ = fs;
        });

        colorW_ = colorProfile->getWidth();
        colorH_ = colorProfile->getHeight();
        depthW_ = depthProfile->getWidth();
        depthH_ = depthProfile->getHeight();

        cameraReady_ = true;
        return true;
    } catch (ob::Error &e) {
        ofLogError("OrbbecAdapter") << "error cámara: " << e.what();
        return false;
    } catch (std::exception &e) {
        ofLogError("OrbbecAdapter") << "error: " << e.what();
        return false;
    }
}

// ---------------------------------------------------------------------------
// Grab the latest color + depth frames; store raw aligned depth.
// ---------------------------------------------------------------------------
void OrbbecAdapter::grabLatestFrame() {
    if (!cameraReady_) return;

    std::shared_ptr<ob::FrameSet> fs;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        fs = latestFrames_;
        latestFrames_ = nullptr;
    }
    if (!fs) return;

    // Store depth (mm) aligned to color, so joints can sample real z.
    auto depthFrame = fs->getDepthFrame();
    if (depthFrame) {
        int w = depthFrame->getWidth(), h = depthFrame->getHeight();
        if (depthFrame->getFormat() == OB_FORMAT_Y16 && w > 0 && h > 0) {
            const uint16_t *data = reinterpret_cast<const uint16_t *>(depthFrame->getData());
            depthW_ = w;
            depthH_ = h;
            depthData_.assign(data, data + w * h);

            // Publicar el depth submuestreado para la silueta real del cuerpo.
            DepthStore::instance().update(depthData_, depthW_, depthH_);
        }
    }

    // Color: guardar textura RGB (para vista de cámara) + subsample al detector.
    auto colorFrame = fs->getColorFrame();
    if (colorFrame) {
        int w = colorFrame->getWidth(), h = colorFrame->getHeight();
        if (colorFrame->getFormat() == OB_FORMAT_RGB && w > 0 && h > 0) {
            const uint8_t *src = static_cast<const uint8_t *>(colorFrame->getData());

            // Textura RGB completa (para la vista de cámara).
            colorPixels_.setFromPixels(src, w, h, OF_IMAGE_COLOR);
            colorTex_.loadData(colorPixels_);

            const int dw = 480, dh = 270;
            std::vector<unsigned char> rgb(dw * dh * 3);
            for (int y = 0; y < dh; y++) {
                int sy = y * h / dh;
                const uint8_t *srcRow = src + sy * w * 3;
                for (int x = 0; x < dw; x++) {
                    int sx = x * w / dw;
                    int di = (y * dw + x) * 3;
                    int si = sx * 3;
                    rgb[di]     = srcRow[si];
                    rgb[di + 1] = srcRow[si + 1];
                    rgb[di + 2] = srcRow[si + 2];
                }
            }
            pushFrameToDetector(rgb, dw, dh);
        }
    }
}

void OrbbecAdapter::pushFrameToDetector(const std::vector<unsigned char> &rgb, int w, int h) {
    std::lock_guard<std::mutex> lock(pendingMutex_);
    pendingFrame_ = rgb;
    pendingW_ = w;
    pendingH_ = h;
    hasPending_ = true;
}

// ---------------------------------------------------------------------------
// Detector thread: TCP → pose_detector.py, receives 33 landmarks.
// ---------------------------------------------------------------------------
void OrbbecAdapter::detectorLoop() {
    while (detectorRunning_) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1000)); continue; }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(detectorPort_);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (connect(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
            close(sock);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        while (detectorRunning_) {
            if (!hasPending_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            std::vector<unsigned char> frame;
            int fw = 0, fh = 0;
            {
                std::lock_guard<std::mutex> lock(pendingMutex_);
                frame = std::move(pendingFrame_);
                pendingFrame_.clear();
                fw = pendingW_; fh = pendingH_;
                hasPending_ = false;
            }

            uint32_t w32 = (uint32_t)fw, h32 = (uint32_t)fh;
            if (::send(sock, &w32, 4, MSG_NOSIGNAL) != 4) break;
            if (::send(sock, &h32, 4, MSG_NOSIGNAL) != 4) break;
            if (::send(sock, frame.data(), frame.size(), MSG_NOSIGNAL) != (ssize_t)frame.size()) break;

            uint32_t rlen = 0;
            auto recvExact = [&](void *buf, size_t n) -> bool {
                char *p = (char *)buf;
                size_t got = 0;
                while (got < n) {
                    ssize_t r = ::recv(sock, p + got, n - got, 0);
                    if (r <= 0) return false;
                    got += r;
                }
                return true;
            };
            if (!recvExact(&rlen, 4)) break;
            if (rlen == 0 || rlen > 10 * 1024 * 1024) break;
            std::string jsonStr(rlen, '\0');
            if (!recvExact(&jsonStr[0], rlen)) break;

            try {
                json j = json::parse(jsonStr);
                // "pose" puede ser:
                //  - (nuevo) una LISTA de poses; cada pose es una lista de 33 landmarks.
                //  - (viejo) una lista plana de 33 landmarks (una sola pose).
                std::vector<std::vector<std::array<float, 3>>> poses;
                std::vector<std::vector<float>> visList;

                auto parseLandmarks = [&](const json& arr) -> std::pair<std::vector<std::array<float,3>>, std::vector<float>> {
                    std::vector<std::array<float, 3>> pts;
                    std::vector<float> vis;
                    for (auto &lm : arr) {
                        pts.push_back({lm.value("x", 0.f), lm.value("y", 0.f), lm.value("z", 0.f)});
                        vis.push_back(lm.value("visibility", 0.f));
                    }
                    return {std::move(pts), std::move(vis)};
                };

                if (j.contains("pose") && j["pose"].is_array() && !j["pose"].empty()) {
                    if (j["pose"][0].is_array()) {
                        // Lista de poses.
                        for (auto &poseArr : j["pose"]) {
                            auto [pts, vis] = parseLandmarks(poseArr);
                            if (pts.size() >= 33) { poses.push_back(std::move(pts)); visList.push_back(std::move(vis)); }
                        }
                    } else if (j["pose"][0].is_object()) {
                        // Lista plana (formato viejo).
                        auto [pts, vis] = parseLandmarks(j["pose"]);
                        if (pts.size() >= 33) { poses.push_back(std::move(pts)); visList.push_back(std::move(vis)); }
                    }
                }
                std::lock_guard<std::mutex> lock(poseMutex_);
                latestPoses_ = std::move(poses);
                latestVisList_ = std::move(visList);
            } catch (std::exception &) {
                // malformed JSON: ignore
            }
        }
        close(sock);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

// ---------------------------------------------------------------------------
// Map 33 MediaPipe landmarks → 18 joints with real depth z.
// ---------------------------------------------------------------------------
JointType OrbbecAdapter::landmarkIndexToJointType(int index) const {
    switch (index) {
        case 0:  return JointType::HEAD;
        case 11: return JointType::SHOULDER_LEFT;
        case 12: return JointType::SHOULDER_RIGHT;
        case 13: return JointType::ELBOW_LEFT;
        case 14: return JointType::ELBOW_RIGHT;
        case 15: return JointType::WRIST_LEFT;
        case 16: return JointType::WRIST_RIGHT;
        case 19: return JointType::HAND_LEFT;
        case 20: return JointType::HAND_RIGHT;
        case 23: return JointType::HIP_LEFT;
        case 24: return JointType::HIP_RIGHT;
        case 25: return JointType::KNEE_LEFT;
        case 26: return JointType::KNEE_RIGHT;
        case 27: return JointType::ANKLE_LEFT;
        case 28: return JointType::ANKLE_RIGHT;
        default: return JointType::COUNT; // invalid
    }
}

float OrbbecAdapter::sampleDepthNormalized(float nx, float ny) const {
    if (depthData_.empty() || depthW_ <= 0 || depthH_ <= 0) return 0.5f;
    int px = (int)(nx * depthW_);
    int py = (int)(ny * depthH_);
    px = std::max(0, std::min(depthW_ - 1, px));
    py = std::max(0, std::min(depthH_ - 1, py));
    uint16_t mm = depthData_[py * depthW_ + px];
    if (mm == 0) return 0.5f;  // invalid / out of sensor range → neutral
    float z = (mm - depthNearMM_) / (depthFarMM_ - depthNearMM_);
    return std::max(0.0f, std::min(1.0f, z));
}

void OrbbecAdapter::applyPose(const std::vector<std::array<float, 3>> &pts,
                              const std::vector<float> &vis, int bodyId) {
    auto lm = [&](int i) -> std::array<float, 3> {
        return (i >= 0 && i < (int)pts.size()) ? pts[i] : std::array<float, 3>{0.5f, 0.5f, 0.0f};
    };
    auto lv = [&](int i) -> float {
        return (i >= 0 && i < (int)vis.size()) ? vis[i] : 0.0f;
    };

    struct Raw { float x, y, conf; };
    Raw raw[static_cast<int>(JointType::COUNT)]{};

    auto setRaw = [&](JointType t, float x, float y, float conf) {
        raw[static_cast<int>(t)] = {x, y, conf};
    };

    setRaw(JointType::HEAD, lm(0)[0], lm(0)[1], lv(0));
    setRaw(JointType::SHOULDER_LEFT, lm(11)[0], lm(11)[1], lv(11));
    setRaw(JointType::SHOULDER_RIGHT, lm(12)[0], lm(12)[1], lv(12));
    setRaw(JointType::ELBOW_LEFT, lm(13)[0], lm(13)[1], lv(13));
    setRaw(JointType::ELBOW_RIGHT, lm(14)[0], lm(14)[1], lv(14));
    setRaw(JointType::WRIST_LEFT, lm(15)[0], lm(15)[1], lv(15));
    setRaw(JointType::WRIST_RIGHT, lm(16)[0], lm(16)[1], lv(16));
    setRaw(JointType::HAND_LEFT, lm(19)[0], lm(19)[1], lv(19));
    setRaw(JointType::HAND_RIGHT, lm(20)[0], lm(20)[1], lv(20));
    setRaw(JointType::HIP_LEFT, lm(23)[0], lm(23)[1], lv(23));
    setRaw(JointType::HIP_RIGHT, lm(24)[0], lm(24)[1], lv(24));
    setRaw(JointType::KNEE_LEFT, lm(25)[0], lm(25)[1], lv(25));
    setRaw(JointType::KNEE_RIGHT, lm(26)[0], lm(26)[1], lv(26));
    setRaw(JointType::ANKLE_LEFT, lm(27)[0], lm(27)[1], lv(27));
    setRaw(JointType::ANKLE_RIGHT, lm(28)[0], lm(28)[1], lv(28));

    float shoMidX = (lm(11)[0] + lm(12)[0]) / 2.f;
    float shoMidY = (lm(11)[1] + lm(12)[1]) / 2.f;
    float hipMidX = (lm(23)[0] + lm(24)[0]) / 2.f;
    float hipMidY = (lm(23)[1] + lm(24)[1]) / 2.f;
    setRaw(JointType::NECK, (lm(0)[0] + shoMidX) / 2.f, (lm(0)[1] + shoMidY) / 2.f,
           std::max(lv(0), std::max(lv(11), lv(12))));
    setRaw(JointType::SPINE_MID, shoMidX, shoMidY, std::max(lv(11), lv(12)));
    setRaw(JointType::SPINE_BASE, hipMidX, hipMidY, std::max(lv(23), lv(24)));

    // Seed desde el frame previo (cuerpo 1) o neutral (cuerpos adicionales).
    currentBody_.bodyId = bodyId;
    currentBody_.isTracked = false;

    static TrackedBody neutral{};
    static bool neutralInit = false;
    if (!neutralInit) {
        for (int i = 0; i < static_cast<int>(JointType::COUNT); ++i) {
            neutral.joints[i].position = {0.5f, 0.5f, 0.5f};
        }
        neutralInit = true;
    }
    const TrackedBody& prev = (bodyId == 1) ? previousBody_ : neutral;

    for (int i = 0; i < static_cast<int>(JointType::COUNT); ++i) {
        currentBody_.joints[i].type       = static_cast<JointType>(i);
        currentBody_.joints[i].position   = prev.joints[i].position;
        currentBody_.joints[i].velocity   = {0.0f, 0.0f, 0.0f};
        currentBody_.joints[i].confidence = 0.0f;
    }

    for (int i = 0; i < static_cast<int>(JointType::COUNT); ++i) {
        Raw r = raw[i];
        if (r.conf < confidenceThreshold_) continue;

        float zReal = sampleDepthNormalized(r.x, r.y);  // depth at raw (unmirrored) pixel
        float newX = 1.0f - r.x;                        // mirror x (existing convention)
        float newY = r.y;
        float newZ = zReal;

        const auto &prevPos = prev.joints[i].position;
        float s = smoothingFactor_;
        currentBody_.joints[i].confidence = r.conf;
        currentBody_.joints[i].position.x = ofLerp(prevPos.x, newX, s);
        currentBody_.joints[i].position.y = ofLerp(prevPos.y, newY, s);
        currentBody_.joints[i].position.z = ofLerp(prevPos.z, newZ, s);
    }

    currentBody_.isTracked = true;
    currentBody_.confidence = 1.0f;
    if (bodyId == 1) previousBody_ = currentBody_;
}

void OrbbecAdapter::fireFrameCallbacks(const TrackedBody& body) {
    if (bodyCallback_) bodyCallback_(body);

    if (jointCallback_) {
        for (int i = 0; i < static_cast<int>(JointType::COUNT); ++i) {
            const auto &joint = body.joints[i];
            if (joint.confidence >= confidenceThreshold_) {
                jointCallback_(body.bodyId, joint.type, joint);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Polling / config
// ---------------------------------------------------------------------------
std::vector<TrackedBody> OrbbecAdapter::getAllTrackedBodies() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return trackedBodies_;
}

Vector3D OrbbecAdapter::getRawJointPosition(int bodyId, JointType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &body : trackedBodies_) {
        if (body.bodyId == bodyId) {
            auto idx = static_cast<int>(type);
            if (idx >= 0 && idx < static_cast<int>(JointType::COUNT)) {
                return body.joints[idx].position;
            }
        }
    }
    return {0.5f, 0.5f, 0.5f};
}

float OrbbecAdapter::getTrackingPresence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return presence_;
}

void OrbbecAdapter::setSmoothing(float factor) {
    std::lock_guard<std::mutex> lock(mutex_);
    smoothingFactor_ = factor;
}

void OrbbecAdapter::setConfidenceThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    confidenceThreshold_ = threshold;
}

void OrbbecAdapter::setZoneGrid(int /*cols*/, int /*rows*/) {
    // Zone grid not applicable — no-op.
}

void OrbbecAdapter::setDetectorPort(int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    detectorPort_ = port;
}

void OrbbecAdapter::setDepthRange(float nearMM, float farMM) {
    std::lock_guard<std::mutex> lock(mutex_);
    depthNearMM_ = nearMM;
    depthFarMM_ = farMM;
}

const std::vector<uint16_t> &OrbbecAdapter::getDepthData() const {
    return depthData_;
}

int OrbbecAdapter::getDepthWidth() const {
    return depthW_;
}

int OrbbecAdapter::getDepthHeight() const {
    return depthH_;
}

// ---------------------------------------------------------------------------
// Callback registration
// ---------------------------------------------------------------------------
void OrbbecAdapter::setJointCallback(JointCallback cb)   { std::lock_guard<std::mutex> lock(mutex_); jointCallback_   = std::move(cb); }
void OrbbecAdapter::setBodyCallback(BodyCallback cb)     { std::lock_guard<std::mutex> lock(mutex_); bodyCallback_    = std::move(cb); }
void OrbbecAdapter::setHandCallback(HandCallback cb)     { std::lock_guard<std::mutex> lock(mutex_); handCallback_    = std::move(cb); }
void OrbbecAdapter::setGestureCallback(GestureCallback cb) { std::lock_guard<std::mutex> lock(mutex_); gestureCallback_ = std::move(cb); }
void OrbbecAdapter::setZoneCallback(ZoneCallback cb)     { std::lock_guard<std::mutex> lock(mutex_); zoneCallback_    = std::move(cb); }

} // namespace modular
