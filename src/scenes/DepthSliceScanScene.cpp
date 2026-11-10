#include "scenes/DepthSliceScanScene.h"
#include "mapping/ParameterTypes.h"
#include <algorithm>

namespace modular {

DepthSliceScanScene::DepthSliceScanScene() {}
DepthSliceScanScene::~DepthSliceScanScene() {}

void DepthSliceScanScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);

    // 6 planos Z desde cerca (+400) hasta lejos (-800).
    planes_ = {
        {  400.0f, 100.0f },
        {  160.0f, 160.0f },
        {  -80.0f, 220.0f },
        { -320.0f, 280.0f },
        { -560.0f, 340.0f },
        { -800.0f, 400.0f },
    };
}

void DepthSliceScanScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Depth Slice Scan");
    ofSetFrameRate(60);
    ofBackground(0);
}

void DepthSliceScanScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
}

void DepthSliceScanScene::pause() { isPaused_ = true; }
void DepthSliceScanScene::resume() { isPaused_ = false; }

void DepthSliceScanScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);
    scanTime_ += dt;
    for (auto& e : interactionPoints_) e.second.update(dt);
}

void DepthSliceScanScene::draw() {
    if (!isActive_) return;

    const float w = ofGetWidth();
    const float h = ofGetHeight();
    const float now = ofGetElapsedTimef();

    ofEnableAlphaBlending();
    ofEnableDepthTest();
    ofEnableBlendMode(OF_BLENDMODE_ADD);

    beginGenesisCamera(cam_, w, h);

    // Pecho (SPINE_MID) para el core motif.
    ofVec3f chest(0.0f, 0.0f, 0.0f);
    {
        auto it = interactionPoints_.find("spine_mid");
        if (it != interactionPoints_.end() && it->second.active) {
            chest = it->second.worldPos(w, h);
        }
    }

    // Por cada plano: activación según la proximidad del cuerpo a su Z.
    for (const auto& plane : planes_) {
        float minDist = 1e9f;
        for (const auto& e : interactionPoints_) {
            if (!e.second.active) continue;
            float jz = e.second.worldPos(w, h).z;
            float d = std::abs(jz - plane.z);
            if (d < minDist) minDist = d;
        }
        float activation = (minDist < 1e9f)
            ? ofClamp(1.0f - minDist / 400.0f, 0.0f, 1.0f) : 0.0f;

        // Color: planos cercanos → neon, lejanos → magenta.
        float t = ofMap(plane.z, kWorldNearZ, kWorldFarZ, 0.0f, 1.0f);
        ofColor col = cNeon().getLerped(cMagenta(), t);

        ofPushMatrix();
        ofTranslate(0.0f, 0.0f, plane.z);
        ofNoFill();
        const int rings = 4;
        for (int r = 0; r < rings; r++) {
            float base = plane.baseRadius + r * 60.0f;
            float pulse = 18.0f * sin(now * 2.0f + r * 1.3f + plane.z * 0.01f);
            float radius = base + pulse + activation * 140.0f;
            float a = (60.0f + 200.0f * activation) * (1.0f - r * 0.18f);
            ofSetColor(col, ofClamp(a, 0.0f, 255.0f));
            ofSetLineWidth(1.5f);
            ofDrawCircle(0.0f, 0.0f, radius);
        }
        ofPopMatrix();
    }

    if (chest.x != 0.0f || chest.y != 0.0f || chest.z != 0.0f) {
        drawCoreMotif(chest, now);
    }

    cam_.end();

    ofDisableBlendMode();
    ofDisableDepthTest();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string DepthSliceScanScene::getId() const { return "depthSliceScan"; }
std::string DepthSliceScanScene::getName() const { return "Depth Slice Scan"; }
std::vector<std::string> DepthSliceScanScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> DepthSliceScanScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void DepthSliceScanScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> DepthSliceScanScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool DepthSliceScanScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool DepthSliceScanScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool DepthSliceScanScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> DepthSliceScanScene::getSuggestedMappings() const { return {}; }
Preset DepthSliceScanScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata DepthSliceScanScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Fase I (Origen): planos Z estáticos que se encienden al paso del cuerpo (topografía de origen).";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"3d", "depth", "scan", "volumetric", "genesis"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = true; m.estimatedDuration = 0;
    return m;
}
void DepthSliceScanScene::onBeat(int) {} void DepthSliceScanScene::onBar(int) {}
bool DepthSliceScanScene::isValid() const { return true; }
std::string DepthSliceScanScene::getInvalidReason() const { return ""; }
void DepthSliceScanScene::unloadResources() { planes_.clear(); }
void DepthSliceScanScene::reloadResources() { setup(); }
void DepthSliceScanScene::keyPressed(int) {} void DepthSliceScanScene::keyReleased(int) {}
void DepthSliceScanScene::mousePressed(int, int, int) {} void DepthSliceScanScene::mouseReleased(int, int, int) {}
void DepthSliceScanScene::mouseDragged(int, int, int) {} void DepthSliceScanScene::mouseMoved(int, int) {}
void DepthSliceScanScene::windowResized(int, int) {}

ScenePtr CreateDepthSliceScanScene() { return std::make_shared<DepthSliceScanScene>(); }

} // namespace modular
