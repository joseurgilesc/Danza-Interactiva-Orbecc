#include "scenes/SilhouetteDepthTunnelScene.h"
#include "mapping/ParameterTypes.h"
#include "input/sensors/DepthStore.h"
#include <algorithm>

namespace modular {

// Huesos del esqueleto (pares de joints que se conectan).
static const std::pair<const char*, const char*> kBones[] = {
    {"head", "neck"},
    {"neck", "shoulder_left"}, {"neck", "shoulder_right"},
    {"shoulder_left", "elbow_left"}, {"elbow_left", "wrist_left"}, {"wrist_left", "hand_left"},
    {"shoulder_right", "elbow_right"}, {"elbow_right", "wrist_right"}, {"wrist_right", "hand_right"},
    {"neck", "spine_mid"}, {"spine_mid", "spine_base"},
    {"spine_base", "hip_left"}, {"spine_base", "hip_right"},
    {"hip_left", "knee_left"}, {"knee_left", "ankle_left"},
    {"hip_right", "knee_right"}, {"knee_right", "ankle_right"},
};

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
    ofSetWindowTitle("Body Joints");
    ofSetFrameRate(60);
    ofBackground(0);
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

    auto world = [&](const std::string& name, ofVec3f& out) -> bool {
        auto it = interactionPoints_.find(name);
        if (it == interactionPoints_.end() || !it->second.active) return false;
        out = it->second.worldPos(w, h);
        return true;
    };

    // Huesos: líneas que conectan articulaciones (color por proximidad media).
    ofNoFill();
    ofSetLineWidth(2.0f);
    for (const auto& bone : kBones) {
        ofVec3f a, b;
        if (!world(bone.first, a) || !world(bone.second, b)) continue;
        float prox = 0.5f * ((1.0f - interactionPoints_[bone.first].z) + (1.0f - interactionPoints_[bone.second].z));
        ofColor col = proximityColor(prox);
        ofSetColor(col, 140);
        ofDrawLine(a, b);
    }

    // Articulaciones: esferas brillantes (tamaño según confianza, color por proximidad).
    ofFill();
    for (const auto& e : interactionPoints_) {
        if (!e.second.active) continue;
        ofVec3f p = e.second.worldPos(w, h);
        float prox = 1.0f - e.second.z;
        ofColor col = proximityColor(prox);
        float pulse = 1.0f + 0.15f * sin(now * 3.0f + p.x * 0.01f);
        float r = (4.0f + 8.0f * prox) * pulse;
        ofSetColor(col, 200);
        ofDrawSphere(p, r);
        // Halo suave.
        ofSetColor(col, 50);
        ofDrawSphere(p, r * 2.2f);
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
std::string SilhouetteDepthTunnelScene::getName() const { return "Body Joints"; }
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
    m.description = "Fase IV (Trascendencia): constelación de joints del cuerpo (esferas + huesos) con profundidad real.";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"3d", "joints", "skeleton", "depth", "volumetric"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = false; m.estimatedDuration = 0;
    return m;
}
void SilhouetteDepthTunnelScene::onBeat(int) {} void SilhouetteDepthTunnelScene::onBar(int) {}
bool SilhouetteDepthTunnelScene::isValid() const { return true; }
std::string SilhouetteDepthTunnelScene::getInvalidReason() const { return ""; }
void SilhouetteDepthTunnelScene::unloadResources() { interactionPoints_.clear(); }
void SilhouetteDepthTunnelScene::reloadResources() { setup(); }
void SilhouetteDepthTunnelScene::keyPressed(int) {} void SilhouetteDepthTunnelScene::keyReleased(int) {}
void SilhouetteDepthTunnelScene::mousePressed(int, int, int) {} void SilhouetteDepthTunnelScene::mouseReleased(int, int, int) {}
void SilhouetteDepthTunnelScene::mouseDragged(int, int, int) {} void SilhouetteDepthTunnelScene::mouseMoved(int, int) {}
void SilhouetteDepthTunnelScene::windowResized(int, int) {}

ScenePtr CreateSilhouetteDepthTunnelScene() { return std::make_shared<SilhouetteDepthTunnelScene>(); }

} // namespace modular
