#include "scenes/AcousticDepthEchoesScene.h"
#include "mapping/ParameterTypes.h"
#include <algorithm>

namespace modular {

AcousticDepthEchoesScene::AcousticDepthEchoesScene() {}
AcousticDepthEchoesScene::~AcousticDepthEchoesScene() {}

void AcousticDepthEchoesScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);
    echoes_.reserve(maxEchoes_);
}

void AcousticDepthEchoesScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Acoustic Depth Echoes");
    ofSetFrameRate(60);
    ofBackground(0);
}

void AcousticDepthEchoesScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
    lastPos_.clear();
    lastSpeedZ_.clear();
}

void AcousticDepthEchoesScene::pause() { isPaused_ = true; }
void AcousticDepthEchoesScene::resume() { isPaused_ = false; }

void AcousticDepthEchoesScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);
    for (auto& e : interactionPoints_) e.second.update(dt);

    const float w = ofGetWidth();
    const float h = ofGetHeight();

    // Detectar frenado brusco en Z por joint.
    for (auto& e : interactionPoints_) {
        if (!e.second.active) { lastPos_.erase(e.first); lastSpeedZ_.erase(e.first); continue; }
        ofVec3f cur = e.second.worldPos(w, h);
        auto pit = lastPos_.find(e.first);
        auto sit = lastSpeedZ_.find(e.first);
        if (pit != lastPos_.end() && sit != lastSpeedZ_.end()) {
            float curSpeedZ = std::abs(cur.z - pit->second.z);
            float prevSpeedZ = sit->second;
            // Frenado brusco: venía rápido en Z y ahora va lento.
            if (prevSpeedZ > fastThreshold_ && curSpeedZ < slowThreshold_) {
                if (echoes_.size() < maxEchoes_) {
                    Echo ec;
                    ec.pos = cur;
                    ec.radius = 10.0f;
                    ec.maxRadius = ofRandom(120.0f, 260.0f);
                    ec.alpha = 1.0f;
                    echoes_.push_back(ec);
                }
            }
            lastSpeedZ_[e.first] = curSpeedZ;
        } else {
            lastSpeedZ_[e.first] = 0.0f;
        }
        lastPos_[e.first] = cur;
    }

    // Crecer radio y desvanecer.
    for (auto& ec : echoes_) {
        ec.radius += 120.0f * dt;
        ec.alpha *= (1.0f - 0.6f * dt);
        if (ec.alpha < 0.0f) ec.alpha = 0.0f;
    }
    echoes_.erase(std::remove_if(echoes_.begin(), echoes_.end(),
        [](const Echo& e) { return e.alpha <= 0.001f || e.radius >= e.maxRadius; }), echoes_.end());
}

void AcousticDepthEchoesScene::draw() {
    if (!isActive_) return;

    const float w = ofGetWidth();
    const float h = ofGetHeight();
    const float now = ofGetElapsedTimef();

    ofEnableAlphaBlending();
    ofEnableDepthTest();
    ofEnableBlendMode(OF_BLENDMODE_ADD);

    beginGenesisCamera(cam_, w, h);

    // Esferas wireframe, cCore fundiendo a cNoche.
    ofNoFill();
    for (const auto& ec : echoes_) {
        float t = ofClamp(ec.radius / ec.maxRadius, 0.0f, 1.0f);
        ofColor col = cCore().getLerped(cNoche(), t);
        ofSetColor(col, ofClamp(ec.alpha * 180.0f, 0.0f, 255.0f));
        ofSetLineWidth(1.2f);
        ofDrawSphere(ec.pos, ec.radius);
    }

    auto it = interactionPoints_.find("spine_mid");
    if (it != interactionPoints_.end() && it->second.active) {
        drawCoreMotif(it->second.worldPos(w, h), now);
    }

    cam_.end();

    ofDisableBlendMode();
    ofDisableDepthTest();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string AcousticDepthEchoesScene::getId() const { return "acousticDepthEchoes"; }
std::string AcousticDepthEchoesScene::getName() const { return "Acoustic Depth Echoes"; }
std::vector<std::string> AcousticDepthEchoesScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> AcousticDepthEchoesScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void AcousticDepthEchoesScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> AcousticDepthEchoesScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool AcousticDepthEchoesScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool AcousticDepthEchoesScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool AcousticDepthEchoesScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> AcousticDepthEchoesScene::getSuggestedMappings() const { return {}; }
Preset AcousticDepthEchoesScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata AcousticDepthEchoesScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Fase IV (Trascendencia): esferas wireframe que nacen del frenado brusco en Z (resonancia final).";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"3d", "echo", "resonance", "depth", "volumetric"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = true; m.estimatedDuration = 0;
    return m;
}
void AcousticDepthEchoesScene::onBeat(int) {} void AcousticDepthEchoesScene::onBar(int) {}
bool AcousticDepthEchoesScene::isValid() const { return true; }
std::string AcousticDepthEchoesScene::getInvalidReason() const { return ""; }
void AcousticDepthEchoesScene::unloadResources() { echoes_.clear(); }
void AcousticDepthEchoesScene::reloadResources() { setup(); }
void AcousticDepthEchoesScene::keyPressed(int) {} void AcousticDepthEchoesScene::keyReleased(int) {}
void AcousticDepthEchoesScene::mousePressed(int, int, int) {} void AcousticDepthEchoesScene::mouseReleased(int, int, int) {}
void AcousticDepthEchoesScene::mouseDragged(int, int, int) {} void AcousticDepthEchoesScene::mouseMoved(int, int) {}
void AcousticDepthEchoesScene::windowResized(int, int) {}

ScenePtr CreateAcousticDepthEchoesScene() { return std::make_shared<AcousticDepthEchoesScene>(); }

} // namespace modular
