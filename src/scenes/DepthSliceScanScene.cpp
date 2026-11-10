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
    ripples_.reserve(maxRipples_);
}

void DepthSliceScanScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Body Energy Waves");
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
    for (auto& e : interactionPoints_) e.second.update(dt);

    const float w = ofGetWidth();
    const float h = ofGetHeight();

    // Emitir ondas desde cada articulación activa (rate-limitado).
    frameCount_++;
    if (frameCount_ % emitEvery_ == 0) {
        for (const auto& e : interactionPoints_) {
            if (!e.second.active) continue;
            if (ripples_.size() >= maxRipples_) break;
            Ripple r;
            r.pos = e.second.worldPos(w, h);
            r.radius = 4.0f;
            r.maxRadius = ofRandom(120.0f, 260.0f);
            r.alpha = 1.0f;
            r.color = proximityColor(1.0f - e.second.z);
            ripples_.push_back(r);
        }
    }

    // Expandir + desvanecer.
    for (auto& r : ripples_) {
        r.radius += 160.0f * dt;
        r.alpha *= (1.0f - 0.7f * dt);
        if (r.alpha < 0.0f) r.alpha = 0.0f;
    }
    ripples_.erase(std::remove_if(ripples_.begin(), ripples_.end(),
        [](const Ripple& r) { return r.alpha <= 0.001f || r.radius >= r.maxRadius; }), ripples_.end());
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

    // Anillos expansivos en el plano XY a la profundidad de cada onda.
    ofNoFill();
    for (const auto& r : ripples_) {
        float a = ofClamp(r.alpha * 200.0f, 0.0f, 255.0f);
        ofPushMatrix();
        ofTranslate(r.pos.x, r.pos.y, r.pos.z);
        ofSetColor(r.color, a);
        ofSetLineWidth(1.8f);
        ofDrawCircle(0, 0, r.radius);
        ofSetColor(r.color, a * 0.35f);
        ofSetLineWidth(4.0f);
        ofDrawCircle(0, 0, r.radius * 0.85f);
        ofPopMatrix();
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
std::string DepthSliceScanScene::getId() const { return "depthSliceScan"; }
std::string DepthSliceScanScene::getName() const { return "Body Energy Waves"; }
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
    m.description = "Fase I (Origen): ondas de energía que emanan de las articulaciones del cuerpo.";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"3d", "ripples", "energy", "depth", "volumetric"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = true; m.estimatedDuration = 0;
    return m;
}
void DepthSliceScanScene::onBeat(int) {} void DepthSliceScanScene::onBar(int) {}
bool DepthSliceScanScene::isValid() const { return true; }
std::string DepthSliceScanScene::getInvalidReason() const { return ""; }
void DepthSliceScanScene::unloadResources() { ripples_.clear(); }
void DepthSliceScanScene::reloadResources() { setup(); }
void DepthSliceScanScene::keyPressed(int) {} void DepthSliceScanScene::keyReleased(int) {}
void DepthSliceScanScene::mousePressed(int, int, int) {} void DepthSliceScanScene::mouseReleased(int, int, int) {}
void DepthSliceScanScene::mouseDragged(int, int, int) {} void DepthSliceScanScene::mouseMoved(int, int) {}
void DepthSliceScanScene::windowResized(int, int) {}

ScenePtr CreateDepthSliceScanScene() { return std::make_shared<DepthSliceScanScene>(); }

} // namespace modular
