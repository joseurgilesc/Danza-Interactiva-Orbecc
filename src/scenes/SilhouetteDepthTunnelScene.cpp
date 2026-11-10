#include "scenes/SilhouetteDepthTunnelScene.h"
#include "mapping/ParameterTypes.h"
#include "input/sensors/DepthStore.h"
#include <algorithm>

namespace modular {

SilhouetteDepthTunnelScene::SilhouetteDepthTunnelScene() {}
SilhouetteDepthTunnelScene::~SilhouetteDepthTunnelScene() {}

void SilhouetteDepthTunnelScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);
}

void SilhouetteDepthTunnelScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Silhouette Depth Tunnel");
    ofSetFrameRate(60);
    ofBackground(0);
    history_.clear();
    sinceRing_ = 0.0f;
}

void SilhouetteDepthTunnelScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
}

void SilhouetteDepthTunnelScene::pause() { isPaused_ = true; }
void SilhouetteDepthTunnelScene::resume() { isPaused_ = false; }

void SilhouetteDepthTunnelScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);
    for (auto& e : interactionPoints_) e.second.update(dt);

    const float w = ofGetWidth();
    const float h = ofGetHeight();

    // Silueta REAL del depth (contorno píxel a píxel) o convex hull de joints.
    auto& depth = DepthStore::instance();
    std::vector<glm::vec2> pts;
    ofVec3f chest(0.0f, 0.0f, 0.0f);
    bool hasChest = false;

    for (const auto& e : interactionPoints_) {
        if (!e.second.active) continue;
        if (e.first == "spine_mid") { chest = e.second.worldPos(w, h); hasChest = true; }
    }

    if (depth.hasData()) {
        auto cps = depth.contourPoints();
        pts.reserve(cps.size());
        for (auto& cp : cps) {
            pts.push_back(glm::vec2(
                ofMap(cp.x, 0.0f, 1.0f, -kWorldHalfW, kWorldHalfW),
                ofMap(cp.y, 0.0f, 1.0f, kWorldHalfH, -kWorldHalfH)));
        }
    } else {
        for (const auto& e : interactionPoints_) {
            if (!e.second.active) continue;
            ofVec3f wp = e.second.worldPos(w, h);
            pts.push_back(glm::vec2(wp.x, wp.y));
        }
        pts = computeConvexHull(pts);
    }

    // Emitir un anillo nuevo cada ringEvery_ segundos.
    sinceRing_ += dt;
    if (sinceRing_ >= ringEvery_ && pts.size() >= 3) {
        sinceRing_ = 0.0f;
        TunnelRing ring;
        ring.hull = computeConvexHull(pts);
        ring.z = hasChest ? chest.z : 0.0f;
        ring.alpha = 1.0f;
        history_.push_back(std::move(ring));
        if (history_.size() > maxRings_) history_.pop_front();
    }

    // Retroceder los anillos en Z y desvanecerlos.
    for (auto& ring : history_) {
        ring.z -= tunnelSpeed_ * dt;
        ring.alpha *= (1.0f - 0.15f * dt);
        if (ring.alpha < 0.0f) ring.alpha = 0.0f;
    }
    while (!history_.empty() && history_.front().z < kWorldFarZ) {
        history_.pop_front();
    }
}

void SilhouetteDepthTunnelScene::draw() {
    if (!isActive_) return;

    const float w = ofGetWidth();
    const float h = ofGetHeight();
    const float now = ofGetElapsedTimef();

    ofEnableAlphaBlending();
    ofEnableDepthTest();
    ofEnableBlendMode(OF_BLENDMODE_ADD);

    beginGenesisCamera(cam_, w, h);

    // Dibujar anillos + conectar anillos consecutivos (paredes del túnel).
    ofNoFill();
    ofSetLineWidth(1.0f);

    for (size_t i = 0; i < history_.size(); i++) {
        const TunnelRing& ring = history_[i];
        // Color: frente (nuevo) cNeon, fondo (viejo) cMagenta.
        float age = static_cast<float>(i) / static_cast<float>(history_.size());
        ofColor col = cNeon().getLerped(cMagenta(), age);
        ofSetColor(col, ofClamp(ring.alpha * 180.0f, 0.0f, 255.0f));

        // Dibujar el polígono del anillo en su Z.
        ofPushMatrix();
        ofTranslate(0.0f, 0.0f, ring.z);
        ofBeginShape();
        for (const auto& v : ring.hull) ofVertex(v.x, v.y);
        ofEndShape(true);  // cerrado
        ofPopMatrix();

        // Conectar con el anillo anterior (paredes del túnel).
        if (i > 0) {
            const TunnelRing& prev = history_[i - 1];
            size_t n = std::min(ring.hull.size(), prev.hull.size());
            for (size_t k = 0; k < n; k++) {
                ofDrawLine(prev.hull[k].x, prev.hull[k].y, prev.z,
                           ring.hull[k].x, ring.hull[k].y, ring.z);
            }
        }
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
std::string SilhouetteDepthTunnelScene::getId() const { return "silhouetteDepthTunnel"; }
std::string SilhouetteDepthTunnelScene::getName() const { return "Silhouette Depth Tunnel"; }
std::vector<std::string> SilhouetteDepthTunnelScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> SilhouetteDepthTunnelScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void SilhouetteDepthTunnelScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> SilhouetteDepthTunnelScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool SilhouetteDepthTunnelScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool SilhouetteDepthTunnelScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool SilhouetteDepthTunnelScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> SilhouetteDepthTunnelScene::getSuggestedMappings() const { return {}; }
Preset SilhouetteDepthTunnelScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata SilhouetteDepthTunnelScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Fase IV (Trascendencia): la silueta (convex hull) se proyecta hacia atrás en Z como un túnel volumétrico.";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"3d", "silhouette", "tunnel", "hull", "volumetric"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = true; m.estimatedDuration = 0;
    return m;
}
void SilhouetteDepthTunnelScene::onBeat(int) {} void SilhouetteDepthTunnelScene::onBar(int) {}
bool SilhouetteDepthTunnelScene::isValid() const { return true; }
std::string SilhouetteDepthTunnelScene::getInvalidReason() const { return ""; }
void SilhouetteDepthTunnelScene::unloadResources() { history_.clear(); }
void SilhouetteDepthTunnelScene::reloadResources() { setup(); }
void SilhouetteDepthTunnelScene::keyPressed(int) {} void SilhouetteDepthTunnelScene::keyReleased(int) {}
void SilhouetteDepthTunnelScene::mousePressed(int, int, int) {} void SilhouetteDepthTunnelScene::mouseReleased(int, int, int) {}
void SilhouetteDepthTunnelScene::mouseDragged(int, int, int) {} void SilhouetteDepthTunnelScene::mouseMoved(int, int) {}
void SilhouetteDepthTunnelScene::windowResized(int, int) {}

ScenePtr CreateSilhouetteDepthTunnelScene() { return std::make_shared<SilhouetteDepthTunnelScene>(); }

} // namespace modular
