#include "scenes/VolumetricLatticeScene.h"
#include "mapping/ParameterTypes.h"
#include <algorithm>

namespace modular {

VolumetricLatticeScene::VolumetricLatticeScene() {}
VolumetricLatticeScene::~VolumetricLatticeScene() {}

void VolumetricLatticeScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);
}

void VolumetricLatticeScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Volumetric Lattice");
    ofSetFrameRate(60);
    ofBackground(0);
}

void VolumetricLatticeScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
}

void VolumetricLatticeScene::pause() { isPaused_ = true; }
void VolumetricLatticeScene::resume() { isPaused_ = false; }

void VolumetricLatticeScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);
    for (auto& e : interactionPoints_) e.second.update(dt);

    const float w = ofGetWidth();
    const float h = ofGetHeight();

    // Recoger articulaciones activas en 3D + su proximidad.
    struct Joint3D { ofVec3f pos; float proximity; };
    std::vector<Joint3D> joints;
    for (const auto& e : interactionPoints_) {
        if (!e.second.active) continue;
        Joint3D j;
        j.pos = e.second.worldPos(w, h);
        j.proximity = 1.0f - e.second.z;
        joints.push_back(j);
    }

    // Reconstruir la malla de líneas conectando pares por distancia.
    latticeMesh_.clear();
    latticeMesh_.setMode(OF_PRIMITIVE_LINES);

    const float th = connectionThreshold_;
    for (size_t i = 0; i < joints.size(); i++) {
        for (size_t k = i + 1; k < joints.size(); k++) {
            float d = (joints[i].pos - joints[k].pos).length();
            if (d > th) continue;

            ofColor ci = proximityColor(joints[i].proximity);
            ofColor ck = proximityColor(joints[k].proximity);
            latticeMesh_.addVertex(joints[i].pos); latticeMesh_.addColor(ci);
            latticeMesh_.addVertex(joints[k].pos); latticeMesh_.addColor(ck);
        }
    }
}

void VolumetricLatticeScene::draw() {
    if (!isActive_) return;

    const float w = ofGetWidth();
    const float h = ofGetHeight();
    const float now = ofGetElapsedTimef();

    ofEnableAlphaBlending();
    ofEnableDepthTest();
    ofEnableBlendMode(OF_BLENDMODE_ADD);   // glow aditivo en las líneas

    beginGenesisCamera(cam_, w, h);

    ofSetLineWidth(1.2f);
    latticeMesh_.draw();

    ofDisableBlendMode();

    // Pecho (SPINE_MID) para el core motif.
    auto it = interactionPoints_.find("spine_mid");
    if (it != interactionPoints_.end() && it->second.active) {
        drawCoreMotif(it->second.worldPos(w, h), now);
    }

    cam_.end();

    ofDisableDepthTest();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string VolumetricLatticeScene::getId() const { return "volumetricLattice"; }
std::string VolumetricLatticeScene::getName() const { return "Volumetric Lattice"; }
std::vector<std::string> VolumetricLatticeScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> VolumetricLatticeScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void VolumetricLatticeScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> VolumetricLatticeScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool VolumetricLatticeScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool VolumetricLatticeScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool VolumetricLatticeScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> VolumetricLatticeScene::getSuggestedMappings() const { return {}; }
Preset VolumetricLatticeScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata VolumetricLatticeScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Fase I (Origen): malla cristalina 3D que conecta articulaciones por distancia (cristalización del ser).";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"3d", "lattice", "mesh", "volumetric", "genesis"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = false; m.estimatedDuration = 0;
    return m;
}
void VolumetricLatticeScene::onBeat(int) {} void VolumetricLatticeScene::onBar(int) {}
bool VolumetricLatticeScene::isValid() const { return true; }
std::string VolumetricLatticeScene::getInvalidReason() const { return ""; }
void VolumetricLatticeScene::unloadResources() { latticeMesh_.clear(); }
void VolumetricLatticeScene::reloadResources() { setup(); }
void VolumetricLatticeScene::keyPressed(int) {} void VolumetricLatticeScene::keyReleased(int) {}
void VolumetricLatticeScene::mousePressed(int, int, int) {} void VolumetricLatticeScene::mouseReleased(int, int, int) {}
void VolumetricLatticeScene::mouseDragged(int, int, int) {} void VolumetricLatticeScene::mouseMoved(int, int) {}
void VolumetricLatticeScene::windowResized(int, int) {}

ScenePtr CreateVolumetricLatticeScene() { return std::make_shared<VolumetricLatticeScene>(); }

} // namespace modular
