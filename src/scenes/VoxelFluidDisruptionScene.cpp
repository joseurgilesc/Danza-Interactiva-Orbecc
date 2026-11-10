#include "scenes/VoxelFluidDisruptionScene.h"
#include "mapping/ParameterTypes.h"
#include <algorithm>

namespace modular {

VoxelFluidDisruptionScene::VoxelFluidDisruptionScene() {}
VoxelFluidDisruptionScene::~VoxelFluidDisruptionScene() {}

void VoxelFluidDisruptionScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);
    voxels_.reserve(maxVoxels_);
}

void VoxelFluidDisruptionScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Voxel Fluid Disruption");
    ofSetFrameRate(60);
    ofBackground(0);
}

void VoxelFluidDisruptionScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
}

void VoxelFluidDisruptionScene::pause() { isPaused_ = true; }
void VoxelFluidDisruptionScene::resume() { isPaused_ = false; }

void VoxelFluidDisruptionScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);
    for (auto& e : interactionPoints_) e.second.update(dt);

    const float w = ofGetWidth();
    const float h = ofGetHeight();

    // Emitir vóxeles en la posición 3D de cada articulación activa (rate-limitado).
    frameCount_++;
    if (frameCount_ % spawnEvery_ == 0) {
        for (const auto& e : interactionPoints_) {
            if (!e.second.active) continue;
            if (voxels_.size() >= maxVoxels_) break;
            Voxel v;
            v.pos = e.second.worldPos(w, h);
            v.vel = ofVec3f(ofRandom(-0.4f, 0.4f), ofRandom(0.5f, 1.2f), ofRandom(-0.4f, 0.4f));  // flotabilidad en Y
            v.size = ofRandom(5.0f, 14.0f);
            v.alpha = 1.0f;
            v.life = 0.0f;
            v.maxLife = ofRandom(2.0f, 4.0f);
            v.noiseSeed = ofRandom(0.0f, 1000.0f);
            voxels_.push_back(v);
        }
    }

    // Integrar: flotabilidad + ruido Perlin 3D + fade.
    const float time = ofGetElapsedTimef();
    for (auto& v : voxels_) {
        v.life += dt;
        // Deriva por ruido Perlin 3D + flotabilidad ascendente.
        float nx = ofSignedNoise(v.pos.x * 0.004f, v.pos.y * 0.004f, time * 0.4f + v.noiseSeed);
        float ny = ofSignedNoise(v.pos.y * 0.004f, v.pos.z * 0.004f, time * 0.4f + v.noiseSeed);
        float nz = ofSignedNoise(v.pos.z * 0.004f, v.pos.x * 0.004f, time * 0.4f + v.noiseSeed);
        v.vel += ofVec3f(nx, 0.0f, nz) * 0.08f;
        v.vel.y += 0.02f;   // flotabilidad
        v.pos += v.vel;
        v.alpha = 1.0f - (v.life / v.maxLife);
        if (v.alpha < 0.0f) v.alpha = 0.0f;
    }
    voxels_.erase(std::remove_if(voxels_.begin(), voxels_.end(),
        [](const Voxel& v) { return v.life >= v.maxLife; }), voxels_.end());
}

void VoxelFluidDisruptionScene::draw() {
    if (!isActive_) return;

    const float w = ofGetWidth();
    const float h = ofGetHeight();
    const float now = ofGetElapsedTimef();

    ofEnableAlphaBlending();
    ofEnableDepthTest();

    beginGenesisCamera(cam_, w, h);

    // Fill neón + wireframe magenta.
    for (const auto& v : voxels_) {
        float a = ofClamp(v.alpha * 180.0f, 0.0f, 255.0f);
        ofFill();
        ofSetColor(cNeon(), a);
        ofDrawBox(v.pos, v.size);
        ofNoFill();
        ofSetColor(cMagenta(), a * 0.5f);
        ofSetLineWidth(0.8f);
        ofDrawBox(v.pos, v.size);
    }

    auto it = interactionPoints_.find("spine_mid");
    if (it != interactionPoints_.end() && it->second.active) {
        drawCoreMotif(it->second.worldPos(w, h), now);
    }

    cam_.end();

    ofDisableDepthTest();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string VoxelFluidDisruptionScene::getId() const { return "voxelFluidDisruption"; }
std::string VoxelFluidDisruptionScene::getName() const { return "Voxel Fluid Disruption"; }
std::vector<std::string> VoxelFluidDisruptionScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> VoxelFluidDisruptionScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void VoxelFluidDisruptionScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> VoxelFluidDisruptionScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool VoxelFluidDisruptionScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool VoxelFluidDisruptionScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool VoxelFluidDisruptionScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> VoxelFluidDisruptionScene::getSuggestedMappings() const { return {}; }
Preset VoxelFluidDisruptionScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata VoxelFluidDisruptionScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Fase III (Disolución): el cuerpo se liquida en vóxeles 3D con ruido Perlin y flotabilidad.";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"3d", "voxel", "fluid", "volumetric", "genesis"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = true; m.estimatedDuration = 0;
    return m;
}
void VoxelFluidDisruptionScene::onBeat(int) {} void VoxelFluidDisruptionScene::onBar(int) {}
bool VoxelFluidDisruptionScene::isValid() const { return true; }
std::string VoxelFluidDisruptionScene::getInvalidReason() const { return ""; }
void VoxelFluidDisruptionScene::unloadResources() { voxels_.clear(); }
void VoxelFluidDisruptionScene::reloadResources() { setup(); }
void VoxelFluidDisruptionScene::keyPressed(int) {} void VoxelFluidDisruptionScene::keyReleased(int) {}
void VoxelFluidDisruptionScene::mousePressed(int, int, int) {} void VoxelFluidDisruptionScene::mouseReleased(int, int, int) {}
void VoxelFluidDisruptionScene::mouseDragged(int, int, int) {} void VoxelFluidDisruptionScene::mouseMoved(int, int) {}
void VoxelFluidDisruptionScene::windowResized(int, int) {}

ScenePtr CreateVoxelFluidDisruptionScene() { return std::make_shared<VoxelFluidDisruptionScene>(); }

} // namespace modular
