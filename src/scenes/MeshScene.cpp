#include "scenes/MeshScene.h"
#include "mapping/ParameterTypes.h"
#include <algorithm>
#include <cmath>

namespace modular {

// ============================================================================
// BodyShape (péndulo invisible)
// ============================================================================
BodyShape::BodyShape(float linelength, int joints, float gravity, float damping)
    : pendulum(std::make_shared<Pendulum>(linelength, joints, gravity, damping))
    , detachTime(ofRandom(120.0f, 250.0f)) {}

void BodyShape::updatePos(float x, float y, float z, float gravity, float damping, float jitterThreshold) {
    if (state == 2) return;
    this->z = z;

    if (state == 0) {
        glm::vec2 target(x, y);
        if (!hasPos) {
            currentPos = target;
            hasPos = true;
            vel = glm::vec2(0.0f);
        } else {
            float dist = glm::distance(currentPos, target);
            if (dist > jitterThreshold) {
                glm::vec2 prev = currentPos;
                currentPos = glm::mix(currentPos, target, 0.3f);
                vel = currentPos - prev;
            } else {
                vel *= 0.5f;   // temblor de cámara: pierde fuerza
            }
        }
        lifeTime++;
        if (lifeTime > detachTime) {
            state = 1;
            lifeTime = 0.0f;
            vel *= 0.35f;
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

    // Velocidad suavizada (EMA) → estaticidad suave, sin parpadeo.
    speed = ofLerp(speed, glm::length(vel), 0.2f);

    // Heading desde la dirección de movimiento.
    float heading = (glm::length(vel) > 0.001f)
        ? std::atan2(vel.y, vel.x) - glm::half_pi<float>() : 0.0f;
    pendulum->update(heading);
    pendulumPath.push_back(pendulum->getTrail(currentPos));
    if (pendulumPath.size() > 100) pendulumPath.erase(pendulumPath.begin());
}

bool BodyShape::isMoving(float threshold) const {
    return speed > threshold;
}

// ============================================================================
// GridNode
// ============================================================================
bool GridNode::repel(const std::vector<Tip>& tips, float radius, float strength) {
    bool wasRepelled = false;
    for (const auto& tip : tips) {
        glm::vec2 force = pos - tip.pos;
        float dist = glm::length(force);
        if (dist < radius && dist > 0.001f) {
            force /= dist;   // normalizar
            float s = ofMap(dist, 0.0f, radius, strength, 0.0f);
            s *= (0.5f + 1.0f * tip.proximity);   // profundidad: más cerca = más fuerza
            force *= s;
            acc += force;
            wasRepelled = true;
        }
    }
    return wasRepelled;
}

void GridNode::returnToBase() {
    glm::vec2 steer = basePos - pos;
    float d = glm::length(steer);
    if (d > 1.0f) {
        steer /= d;
        float spd = ofMap(d, 0.0f, 200.0f, 1.0f, maxSpeed);
        steer *= spd;
        glm::vec2 steerForce = steer - vel;
        if (glm::length(steerForce) > 1.2f) steerForce = glm::normalize(steerForce) * 1.2f;
        acc += steerForce;
    } else {
        pos = basePos;
        vel *= 0.0f;
    }
}

void GridNode::update(bool isIdle, float dt) {
    if (isIdle) {
        returnToBase();
    } else {
        acc += glm::vec2(ofRandom(-0.5f, 0.5f), ofRandom(-0.5f, 0.5f));
    }
    vel += acc;
    if (glm::length(vel) > maxSpeed) vel = glm::normalize(vel) * maxSpeed;
    pos += vel * dt * 60.0f;
    acc *= 0.0f;
    vel *= (isIdle ? 0.85f : 0.90f);
}

// ============================================================================
// MeshScene
// ============================================================================
MeshScene::MeshScene() {}
MeshScene::~MeshScene() {}

void MeshScene::buildGrid() {
    grid_.clear();
    cols_ = (int)(ofGetWidth() / spacing_) + 1;
    rows_ = (int)(ofGetHeight() / spacing_) + 1;
    for (int r = 0; r < rows_; r++) {
        for (int c = 0; c < cols_; c++) {
            grid_.push_back(GridNode(c * spacing_, r * spacing_));
        }
    }
}

void MeshScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);
    buildGrid();

    emitters_.clear();
    emitters_.push_back({"wrist_left", ""});
    emitters_.push_back({"wrist_right", ""});
    emitters_.push_back({"head", ""});
    emitters_.push_back({"ankle_left", ""});
    emitters_.push_back({"ankle_right", ""});
}

void MeshScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Generative Mesh");
    ofSetFrameRate(60);
    ofBackground(0);
    buildGrid();
}

void MeshScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
    shapes_.clear();
}

void MeshScene::pause() { isPaused_ = true; }
void MeshScene::resume() { isPaused_ = false; }

void MeshScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);
    for (auto& e : interactionPoints_) e.second.update(dt);

    // Actualizar los péndulos invisibles anclados a los joints.
    for (size_t i = 0; i < emitters_.size(); i++) {
        const std::string& jointName = emitters_[i].first;
        auto it = interactionPoints_.find(jointName);
        bool hasFresh = (it != interactionPoints_.end() && it->second.active && it->second.confidence > 0.9f);

        if (i >= shapes_.size()) {
            shapes_.push_back(std::make_shared<BodyShape>(linelength_, joints_, gravity_, damping_));
        }
        auto& shape = shapes_[i];

        if (hasFresh) {
            shape->updatePos(it->second.position.x, it->second.position.y, it->second.z, gravity_, damping_, jitterThreshold_);
        } else if (shape->state == 0 && shape->hasPos) {
            // Joint perdido → soltar.
            shape->state = 1;
            shape->lifeTime = 0.0f;
            shape->vel *= 0.2f;
        }
    }

    // Recoger tips activos (solo joints que se mueven → estaticidad).
    std::vector<Tip> activeTips;
    for (auto& shape : shapes_) {
        if (!shape->isMoving(moveThreshold_)) continue;
        // La punta del péndulo (última posición del trazo).
        if (shape->pendulumPath.empty()) continue;
        Tip t;
        t.pos = shape->pendulumPath.back();
        t.proximity = 1.0f - shape->z;   // profundidad
        activeTips.push_back(t);
    }

    // Repeler los nodos + marcar si hubo disturbio.
    bool disturbed = false;
    for (auto& node : grid_) {
        if (node.repel(activeTips, repulsionRadius_, repulsionStrength_)) disturbed = true;
    }
    if (disturbed) lastDisturbance_ = ofGetElapsedTimeMillis();

    bool isIdle = (ofGetElapsedTimeMillis() - lastDisturbance_ > idleDelayMs_);
    for (auto& node : grid_) {
        node.update(isIdle, dt);
    }
}

void MeshScene::drawBones() {
    static const std::pair<const char*, const char*> kBones[] = {
        {"head", "neck"}, {"neck", "shoulder_left"}, {"neck", "shoulder_right"},
        {"shoulder_left", "elbow_left"}, {"elbow_left", "wrist_left"},
        {"shoulder_right", "elbow_right"}, {"elbow_right", "wrist_right"},
        {"neck", "spine_mid"}, {"spine_mid", "spine_base"},
        {"spine_base", "hip_left"}, {"spine_base", "hip_right"},
        {"hip_left", "knee_left"}, {"knee_left", "ankle_left"},
        {"hip_right", "knee_right"}, {"knee_right", "ankle_right"},
    };
    ofSetColor(cDim().r, cDim().g, cDim().b, 120);
    ofSetLineWidth(1.5f);
    for (const auto& bone : kBones) {
        auto a = interactionPoints_.find(bone.first);
        auto b = interactionPoints_.find(bone.second);
        if (a == interactionPoints_.end() || b == interactionPoints_.end()) continue;
        if (!a->second.active || !b->second.active) continue;
        ofDrawLine(a->second.position.x, a->second.position.y, b->second.position.x, b->second.position.y);
    }
}

void MeshScene::draw() {
    if (!isActive_) return;

    ofEnableAlphaBlending();
    ofEnableBlendMode(OF_BLENDMODE_ADD);

    // Dibujar la red conectada, color por velocidad del nodo.
    for (int r = 0; r < rows_; r++) {
        for (int c = 0; c < cols_; c++) {
            int i = c + r * cols_;
            const GridNode& node = grid_[i];
            float speed = glm::length(node.vel);
            float t = ofClamp(speed / 15.0f, 0.0f, 1.0f);
            ofColor col;
            if (t < 0.5f) col = cNoche().getLerped(cNeon(), t * 2.0f);
            else          col = cNeon().getLerped(cCore(), (t - 0.5f) * 2.0f);
            float alpha = ofMap(speed, 0.0f, 15.0f, 30.0f, 100.0f, true);
            ofSetColor(col, alpha);
            ofSetLineWidth(ofMap(speed, 0.0f, 15.0f, 0.5f, 4.0f, true));

            if (c < cols_ - 1) {
                const GridNode& right = grid_[(c + 1) + r * cols_];
                ofDrawLine(node.pos.x, node.pos.y, right.pos.x, right.pos.y);
            }
            if (r < rows_ - 1) {
                const GridNode& down = grid_[c + (r + 1) * cols_];
                ofDrawLine(node.pos.x, node.pos.y, down.pos.x, down.pos.y);
            }
        }
    }

    ofDisableBlendMode();

    // Esqueleto (sensación de cuerpo).
    drawBones();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string MeshScene::getId() const { return "mesh"; }
std::string MeshScene::getName() const { return "Generative Mesh"; }
std::vector<std::string> MeshScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> MeshScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void MeshScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> MeshScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool MeshScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool MeshScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool MeshScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> MeshScene::getSuggestedMappings() const { return {}; }
Preset MeshScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata MeshScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Generative mesh: péndulos invisibles del cuerpo deforman una red elástica (color por velocidad, profundidad modula la fuerza).";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"mesh", "grid", "generative", "depth", "pendulum"}; m.category = "generative";
    m.hasInternalState = true; m.isTimeBased = true; m.estimatedDuration = 0;
    return m;
}
void MeshScene::onBeat(int) {} void MeshScene::onBar(int) {}
bool MeshScene::isValid() const { return true; }
std::string MeshScene::getInvalidReason() const { return ""; }
void MeshScene::unloadResources() { grid_.clear(); shapes_.clear(); }
void MeshScene::reloadResources() { setup(); }
void MeshScene::keyPressed(int) {} void MeshScene::keyReleased(int) {}
void MeshScene::mousePressed(int, int, int) {} void MeshScene::mouseReleased(int, int, int) {}
void MeshScene::mouseDragged(int, int, int) {} void MeshScene::mouseMoved(int, int) {}
void MeshScene::windowResized(int, int) { buildGrid(); }

ScenePtr CreateMeshScene() { return std::make_shared<MeshScene>(); }

} // namespace modular
