#include "scenes/DepthKineticVortexScene.h"
#include "mapping/ParameterTypes.h"
#include <algorithm>

namespace modular {

DepthKineticVortexScene::DepthKineticVortexScene() {}
DepthKineticVortexScene::~DepthKineticVortexScene() {}

void DepthKineticVortexScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);
    particles_.reserve(maxParticles_);
}

void DepthKineticVortexScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Depth Kinetic Vortex");
    ofSetFrameRate(60);
    ofBackground(0);
}

void DepthKineticVortexScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
    lastPos_.clear();
}

void DepthKineticVortexScene::pause() { isPaused_ = true; }
void DepthKineticVortexScene::resume() { isPaused_ = false; }

void DepthKineticVortexScene::emitBurst(const ofVec3f& pos, float strength) {
    int n = 8;
    for (int i = 0; i < n; i++) {
        if (particles_.size() >= maxParticles_) break;
        KinParticle p;
        p.pos = pos;
        // Explosión radial en 3D, escalada por la fuerza del movimiento.
        float az = ofRandom(0.0f, glm::two_pi<float>());
        float el = ofRandom(-1.0f, 1.0f);
        float r = ofRandom(0.4f, 1.0f);
        p.vel = ofVec3f(
            std::cos(az) * std::sqrt(1.0f - el * el) * r,
            std::sin(az) * std::sqrt(1.0f - el * el) * r,
            el * r
        ) * strength;
        p.life = 0.0f;
        p.maxLife = ofRandom(0.6f, 1.6f);
        particles_.push_back(p);
    }
}

void DepthKineticVortexScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);
    for (auto& e : interactionPoints_) e.second.update(dt);

    const float w = ofGetWidth();
    const float h = ofGetHeight();

    // Detectar aceleración Z por joint y emitir partículas.
    for (auto& e : interactionPoints_) {
        if (!e.second.active) { lastPos_.erase(e.first); continue; }
        ofVec3f cur = e.second.worldPos(w, h);
        auto it = lastPos_.find(e.first);
        if (it != lastPos_.end()) {
            ofVec3f v = cur - it->second;   // desplazamiento por frame
            float vz = std::abs(v.z);
            if (vz > zThreshold_) {
                emitBurst(cur, ofClamp(vz * 0.8f, 2.0f, 20.0f));
            }
        }
        lastPos_[e.first] = cur;
    }

    // Gesto: manos juntas → ráfaga especial (objetos extra) en el punto medio.
    auto hl = interactionPoints_.find("hand_left");
    auto hr = interactionPoints_.find("hand_right");
    if (hl != interactionPoints_.end() && hr != interactionPoints_.end() &&
        hl->second.active && hr->second.active) {
        ofVec3f wl = hl->second.worldPos(w, h);
        ofVec3f wr = hr->second.worldPos(w, h);
        if ((wl - wr).length() < 130.0f) {
            emitBurst((wl + wr) * 0.5f, 28.0f);   // ráfaga grande de "unión"
        }
    }

    // Integrar partículas: fricción + vida.
    for (auto& p : particles_) {
        p.life += dt;
        p.vel *= (1.0f - 1.5f * dt);   // fricción
        p.pos += p.vel * dt * 60.0f;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
        [](const KinParticle& p) { return p.life >= p.maxLife; }), particles_.end());
}

void DepthKineticVortexScene::draw() {
    if (!isActive_) return;

    const float w = ofGetWidth();
    const float h = ofGetHeight();
    const float now = ofGetElapsedTimef();

    ofEnableAlphaBlending();
    ofEnableDepthTest();
    ofEnableBlendMode(OF_BLENDMODE_ADD);

    beginGenesisCamera(cam_, w, h);

    // Estelas: línea desde pos hacia pos - vel. Color por velocidad.
    for (const auto& p : particles_) {
        float speed = p.vel.length();
        float t = ofClamp(speed / 20.0f, 0.0f, 1.0f);   // rápido → cCore, lento → cMagenta
        ofColor col = cMagenta().getLerped(cCore(), t);
        float a = ofMap(p.life / p.maxLife, 0.0f, 1.0f, 200.0f, 0.0f);
        ofSetColor(col, ofClamp(a, 0.0f, 255.0f));
        ofSetLineWidth(3.0f);
        ofDrawLine(p.pos, p.pos - p.vel * 1.5f);
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
std::string DepthKineticVortexScene::getId() const { return "depthKineticVortex"; }
std::string DepthKineticVortexScene::getName() const { return "Depth Kinetic Vortex"; }
std::vector<std::string> DepthKineticVortexScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> DepthKineticVortexScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void DepthKineticVortexScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> DepthKineticVortexScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool DepthKineticVortexScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool DepthKineticVortexScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool DepthKineticVortexScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> DepthKineticVortexScene::getSuggestedMappings() const { return {}; }
Preset DepthKineticVortexScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata DepthKineticVortexScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Fase II (Tensión): partículas 3D que estallan con la aceleración en Z (vórtice de inestabilidad).";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"3d", "particles", "vortex", "depth", "volumetric"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = true; m.estimatedDuration = 0;
    return m;
}
void DepthKineticVortexScene::onBeat(int) {} void DepthKineticVortexScene::onBar(int) {}
bool DepthKineticVortexScene::isValid() const { return true; }
std::string DepthKineticVortexScene::getInvalidReason() const { return ""; }
void DepthKineticVortexScene::unloadResources() { particles_.clear(); }
void DepthKineticVortexScene::reloadResources() { setup(); }
void DepthKineticVortexScene::keyPressed(int) {} void DepthKineticVortexScene::keyReleased(int) {}
void DepthKineticVortexScene::mousePressed(int, int, int) {} void DepthKineticVortexScene::mouseReleased(int, int, int) {}
void DepthKineticVortexScene::mouseDragged(int, int, int) {} void DepthKineticVortexScene::mouseMoved(int, int) {}
void DepthKineticVortexScene::windowResized(int, int) {}

ScenePtr CreateDepthKineticVortexScene() { return std::make_shared<DepthKineticVortexScene>(); }

} // namespace modular
