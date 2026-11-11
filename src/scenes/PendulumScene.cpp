#include "scenes/PendulumScene.h"
#include "mapping/ParameterTypes.h"
#include <algorithm>
#include <cmath>

namespace modular {

// Huesos del esqueleto (brazos, piernas, torso) para la sensación de esqueleto.
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

// ============================================================================
// Pendulum
// ============================================================================
Pendulum::Pendulum(float sz, int h, float g, float d)
    : hierarchy(h - 1), size(sz), gravity(g), damping(d), angle(ofRandom(glm::two_pi<float>())) {
    if (hierarchy > 0) {
        arm = std::make_shared<Pendulum>(sz / 1.5f, hierarchy, g, d);
    }
}

void Pendulum::update(float heading) {
    end = origin + glm::vec2(size * std::sin(angle), size * std::cos(angle));
    float accel = (-gravity / size) * std::sin(angle + heading);
    angle += angularVel;
    angularVel += accel;
    angularVel *= damping;
    if (arm) arm->update(heading);
}

glm::vec2 Pendulum::getTrail(const glm::vec2& offset) {
    if (arm) {
        return arm->getTrail(offset) + end;
    }
    return end + offset;
}

void Pendulum::draw() {
    ofSetColor(255, 255, 255, 40);
    ofDrawLine(origin.x, origin.y, end.x, end.y);
    ofFill();
    ofDrawCircle(end.x, end.y, 2.0f);
    ofNoFill();
    if (arm) {
        ofPushMatrix();
        ofTranslate(end.x, end.y);
        arm->draw();
        ofPopMatrix();
    }
}

// ============================================================================
// PendulumShape
// ============================================================================
PendulumShape::PendulumShape(ofFloatColor c, float linelength, int joints, float gravity, float damping)
    : color(c)
    , pendulum(std::make_shared<Pendulum>(linelength, joints, gravity, damping))
    , detachTime(ofRandom(120.0f, 250.0f)) {}

void PendulumShape::updatePos(float x, float y, float gravity, float damping) {
    if (state == 2) return;

    if (state == 0) {
        glm::vec2 target(x, y);
        if (!hasPos) {
            currentPos = target;
            hasPos = true;
        } else {
            glm::vec2 prev = currentPos;
            currentPos = glm::mix(currentPos, target, 0.3f);
            vel = currentPos - prev;
        }
        lifeTime++;
        if (lifeTime > detachTime) {
            state = 1;
            lifeTime = 0.0f;
            vel *= 0.35f;   // amortiguar la velocidad heredada (se va suave)
        }
    } else if (state == 1) {
        currentPos += vel;
        vel.y -= 0.05f;
        vel *= 0.98f;
        lifeTime++;
        if (lifeTime > 180.0f) {
            state = 2;
            return;
        }
    }

    shapePath.push_back(currentPos);
    if (shapePath.size() > 200) shapePath.erase(shapePath.begin());

    float heading = 0.0f;
    if (shapePath.size() > 1) {
        const glm::vec2& prev = shapePath[shapePath.size() - 2];
        heading = std::atan2(currentPos.y - prev.y, currentPos.x - prev.x) - glm::half_pi<float>();
    }

    pendulum->update(heading);
    pendulumPath.push_back(pendulum->getTrail(currentPos));
    if (pendulumPath.size() > 200) pendulumPath.erase(pendulumPath.begin());
}

void PendulumShape::updateDetached(float gravity) {
    (void)gravity;  // ya se aplica en updatePos (vel.y -= 0.1)
    // updatePos con (0,0) cuando detached — lo maneja updatePos.
}

void PendulumShape::draw(float minThickness, float maxThickness, float speedSensitivity) {
    // Hilo guía (trayectoria del joint).
    ofSetColor(255, 255, 255, 10);
    ofSetLineWidth(0.8f);
    if (shapePath.size() > 1) {
        ofBeginShape();
        for (const auto& p : shapePath) ofVertex(p.x, p.y);
        ofEndShape(false);
    }

    // Péndulo (brazos).
    if (state != 2) {
        ofPushMatrix();
        ofTranslate(currentPos.x, currentPos.y);
        pendulum->draw();
        ofPopMatrix();
    }

    // Trazo del péndulo: grosor según la VELOCIDAD DEL JOINT (no del péndulo).
    if (pendulumPath.size() > 1) {
        ofSetColor(color.r * 255, color.g * 255, color.b * 255, color.a * 255);
        float speed = glm::length(vel);   // velocidad del joint (px/frame)
        float sw = ofClamp(ofMap(speed, 0.0f, speedSensitivity, minThickness, maxThickness, true), minThickness, maxThickness);
        ofSetLineWidth(sw);
        ofBeginShape();
        for (const auto& p : pendulumPath) ofVertex(p.x, p.y);
        ofEndShape(false);
    }
}

// ============================================================================
// PendulumScene
// ============================================================================
PendulumScene::PendulumScene() {}
PendulumScene::~PendulumScene() {}

void PendulumScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);

    // Receptor OSC de Csound (energía/frecuencia del audio).
    if (!csoundReceiverSetup_) {
        csoundReceiver_.setup(9001);
        csoundReceiverSetup_ = true;
    }

    emitters_.clear();
    emitters_.push_back({"wrist_left",  cNeon()});
    emitters_.push_back({"wrist_right", cMagenta()});
    emitters_.push_back({"head",        cCore()});
    emitters_.push_back({"ankle_left",  cNeon()});
    emitters_.push_back({"ankle_right", cCore()});
}

void PendulumScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Generative Pendulum");
    ofSetFrameRate(60);
    ofBackground(0);
}

void PendulumScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
    emitters_.clear();
    floating_.clear();
}

void PendulumScene::pause() { isPaused_ = true; }
void PendulumScene::resume() { isPaused_ = false; }

void PendulumScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);
    for (auto& e : interactionPoints_) e.second.update(dt);

    // Poll Csound OSC → energía/frecuencia del audio para modular lo visual.
    if (csoundReceiverSetup_) {
        while (csoundReceiver_.hasWaitingMessages()) {
            ofxOscMessage m;
            csoundReceiver_.getNextMessage(m);
            if (m.getAddress() == "/csound/energy" && m.getNumArgs() >= 1) {
                audioEnergy_ = ofClamp(m.getArgAsFloat(0), 0.0f, 1.0f);
            } else if (m.getAddress() == "/csound/freq" && m.getNumArgs() >= 1) {
                audioFreq_ = m.getArgAsFloat(0);
            }
        }
    }

    for (auto& emitter : emitters_) {
        auto it = interactionPoints_.find(emitter.jointName);
        bool hasFresh = (it != interactionPoints_.end() && it->second.active && it->second.confidence > 0.9f);

        if (!emitter.active || emitter.active->state != 0) {
            // El shape actual se fue (detached o murió) → pasarlo a floating_ y crear otro.
            if (emitter.active && emitter.active->state == 1) {
                floating_.push_back(emitter.active);
            }
            emitter.active = std::make_shared<PendulumShape>(emitter.color, linelength_, joints_, gravity_, damping_);
        }

        if (hasFresh) {
            emitter.active->updatePos(it->second.position.x, it->second.position.y, gravity_, damping_);
        } else if (emitter.active->state == 0 && emitter.active->hasPos) {
            // Joint perdido → soltar (flota suave, NO vuela con la inercia del decay).
            emitter.active->state = 1;
            emitter.active->lifeTime = 0.0f;
            emitter.active->vel *= 0.2f;
        }
    }

    // Actualizar los shapes flotantes (detached → dead).
    for (auto& fs : floating_) {
        if (fs->state == 1) {
            fs->updatePos(0.0f, 0.0f, gravity_, damping_);
        }
    }
    floating_.erase(std::remove_if(floating_.begin(), floating_.end(),
        [](const std::shared_ptr<PendulumShape>& s) { return s->state == 2; }), floating_.end());
}

void PendulumScene::draw() {
    if (!isActive_) return;

    ofEnableAlphaBlending();

    // Huesos del esqueleto (sensación de cuerpo) — detrás de los péndulos.
    drawBones();

    ofEnableBlendMode(OF_BLENDMODE_ADD);

    for (auto& emitter : emitters_) {
        if (emitter.active) emitter.active->draw(minThickness_, maxThickness_, speedSensitivity_);
    }
    for (auto& fs : floating_) {
        fs->draw(minThickness_, maxThickness_, speedSensitivity_);
    }

    ofDisableBlendMode();
}

void PendulumScene::drawBones() {
    ofSetColor(cDim().r, cDim().g, cDim().b, 130);
    ofSetLineWidth(1.6f);
    for (const auto& bone : kBones) {
        auto a = interactionPoints_.find(bone.first);
        auto b = interactionPoints_.find(bone.second);
        if (a == interactionPoints_.end() || b == interactionPoints_.end()) continue;
        if (!a->second.active || !b->second.active) continue;
        ofDrawLine(a->second.position.x, a->second.position.y, b->second.position.x, b->second.position.y);
    }
    // Puntos en las articulaciones.
    ofFill();
    for (const auto& e : interactionPoints_) {
        if (!e.second.active) continue;
        ofDrawCircle(e.second.position.x, e.second.position.y, 2.5f);
    }
    ofNoFill();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string PendulumScene::getId() const { return "pendulum"; }
std::string PendulumScene::getName() const { return "Generative Pendulum"; }
std::vector<std::string> PendulumScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> PendulumScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void PendulumScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> PendulumScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool PendulumScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool PendulumScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool PendulumScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> PendulumScene::getSuggestedMappings() const { return {}; }
Preset PendulumScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata PendulumScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Péndulos generativos anclados al cuerpo (muñecas, cabeza, tobillos) con trazo por velocidad.";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"pendulum", "generative", "body", "trail"}; m.category = "generative";
    m.hasInternalState = true; m.isTimeBased = true; m.estimatedDuration = 0;
    return m;
}
void PendulumScene::onBeat(int) {} void PendulumScene::onBar(int) {}
bool PendulumScene::isValid() const { return true; }
std::string PendulumScene::getInvalidReason() const { return ""; }
void PendulumScene::unloadResources() { emitters_.clear(); floating_.clear(); }
void PendulumScene::reloadResources() { setup(); }
void PendulumScene::keyPressed(int) {} void PendulumScene::keyReleased(int) {}
void PendulumScene::mousePressed(int, int, int) {} void PendulumScene::mouseReleased(int, int, int) {}
void PendulumScene::mouseDragged(int, int, int) {} void PendulumScene::mouseMoved(int, int) {}
void PendulumScene::windowResized(int, int) {}

ScenePtr CreatePendulumScene() { return std::make_shared<PendulumScene>(); }

} // namespace modular
