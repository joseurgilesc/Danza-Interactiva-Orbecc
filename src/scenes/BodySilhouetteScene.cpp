#include "scenes/BodySilhouetteScene.h"
#include "scenes/GenesisCommon.h"
#include "mapping/ParameterTypes.h"
#include <algorithm>

namespace modular {

BodySilhouetteScene::BodySilhouetteScene() {}
BodySilhouetteScene::~BodySilhouetteScene() {}

void BodySilhouetteScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);
    particles_.reserve(maxParticles_);
}

void BodySilhouetteScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Body Silhouette (depth)");
    ofSetFrameRate(60);
    ofBackground(0);
}

void BodySilhouetteScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
}

void BodySilhouetteScene::pause() { isPaused_ = true; }
void BodySilhouetteScene::resume() { isPaused_ = false; }

void BodySilhouetteScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);

    auto& depth = DepthStore::instance();
    const float w = ofGetWidth();
    const float h = ofGetHeight();

    // Rejection sampling: lanzar puntos aleatorios y quedarse con los que caen
    // dentro de la silueta real del cuerpo.
    if (depth.hasData()) {
        for (int a = 0; a < spawnAttempts_; a++) {
            if (particles_.size() >= maxParticles_) break;
            float nx = ofRandom(0.0f, 1.0f);
            float ny = ofRandom(0.0f, 1.0f);
            if (!depth.isBody(nx, ny)) continue;

            SilParticle p;
            p.pos = glm::vec2(nx * w, ny * h);
            p.size = ofRandom(3.0f, 12.0f);
            p.alpha = 1.0f;
            p.life = 0.0f;
            p.maxLife = ofRandom(1.5f, 3.5f);
            // Color según proximidad (más cerca → cCore, lejos → cNeon).
            float proximity = 1.0f - ofClamp(depth.at(
                (int)(nx * (depth.gridW() - 1)),
                (int)(ny * (depth.gridH() - 1))) / (float)depth.getThreshold(), 0.0f, 1.0f);
            p.color = cNeon().getLerped(cCore(), proximity);
            particles_.push_back(p);
        }
    }

    // Fade + decaimiento.
    for (auto& p : particles_) {
        p.life += dt;
        p.alpha = 1.0f - (p.life / p.maxLife);
        if (p.alpha < 0.0f) p.alpha = 0.0f;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
        [](const SilParticle& p) { return p.life >= p.maxLife; }), particles_.end());
}

void BodySilhouetteScene::draw() {
    if (!isActive_) return;

    ofEnableAlphaBlending();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    ofFill();

    // Glow de 2 capas por partícula.
    for (const auto& p : particles_) {
        float a = p.alpha * 200.0f;
        if (a <= 0.0f) continue;
        ofSetColor(p.color.r * 255, p.color.g * 255, p.color.b * 255, a * 0.4f);
        ofDrawCircle(p.pos.x, p.pos.y, p.size * 2.0f);
        ofSetColor(p.color.r * 255, p.color.g * 255, p.color.b * 255, a);
        ofDrawCircle(p.pos.x, p.pos.y, p.size);
    }

    ofDisableBlendMode();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string BodySilhouetteScene::getId() const { return "bodySilhouette"; }
std::string BodySilhouetteScene::getName() const { return "Body Silhouette"; }
std::vector<std::string> BodySilhouetteScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> BodySilhouetteScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void BodySilhouetteScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> BodySilhouetteScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool BodySilhouetteScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool BodySilhouetteScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool BodySilhouetteScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> BodySilhouetteScene::getSuggestedMappings() const { return {}; }
Preset BodySilhouetteScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata BodySilhouetteScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Rellena la silueta real del cuerpo (desde el depth) con partículas luminosas.";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"silhouette", "depth", "particles", "volumetric"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = true; m.estimatedDuration = 0;
    return m;
}
void BodySilhouetteScene::onBeat(int) {} void BodySilhouetteScene::onBar(int) {}
bool BodySilhouetteScene::isValid() const { return true; }
std::string BodySilhouetteScene::getInvalidReason() const { return ""; }
void BodySilhouetteScene::unloadResources() { particles_.clear(); }
void BodySilhouetteScene::reloadResources() { setup(); }
void BodySilhouetteScene::keyPressed(int) {} void BodySilhouetteScene::keyReleased(int) {}
void BodySilhouetteScene::mousePressed(int, int, int) {} void BodySilhouetteScene::mouseReleased(int, int, int) {}
void BodySilhouetteScene::mouseDragged(int, int, int) {} void BodySilhouetteScene::mouseMoved(int, int) {}
void BodySilhouetteScene::windowResized(int, int) {}

ScenePtr CreateBodySilhouetteScene() { return std::make_shared<BodySilhouetteScene>(); }

} // namespace modular
