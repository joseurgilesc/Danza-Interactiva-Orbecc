#include "scenes/ElasticDepthMeshScene.h"
#include "mapping/ParameterTypes.h"
#include <algorithm>

namespace modular {

ElasticDepthMeshScene::ElasticDepthMeshScene() {}
ElasticDepthMeshScene::~ElasticDepthMeshScene() {}

int ElasticDepthMeshScene::index(int c, int r) const { return r * cols_ + c; }

void ElasticDepthMeshScene::buildGrid() {
    nodes_.clear();
    nodes_.resize(cols_ * rows_);
    for (int r = 0; r < rows_; r++) {
        for (int c = 0; c < cols_; c++) {
            float x = ofMap(c, 0, cols_ - 1, -kWorldHalfW, kWorldHalfW);
            float y = ofMap(r, 0, rows_ - 1, kWorldHalfH, -kWorldHalfH);
            MeshNode& n = nodes_[index(c, r)];
            n.restPos = ofVec3f(x, y, 0.0f);
            n.pos = n.restPos;
            n.vel = ofVec3f(0.0f);
        }
    }
}

void ElasticDepthMeshScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);
    buildGrid();
}

void ElasticDepthMeshScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Elastic Depth Mesh");
    ofSetFrameRate(60);
    ofBackground(0);
}

void ElasticDepthMeshScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
}

void ElasticDepthMeshScene::pause() { isPaused_ = true; }
void ElasticDepthMeshScene::resume() { isPaused_ = false; }

void ElasticDepthMeshScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);
    for (auto& e : interactionPoints_) e.second.update(dt);

    const float w = ofGetWidth();
    const float h = ofGetHeight();

    // Recoger articulaciones activas en 3D.
    struct Joint3D { ofVec3f pos; };
    std::vector<Joint3D> joints;
    for (const auto& e : interactionPoints_) {
        if (!e.second.active) continue;
        Joint3D j;
        j.pos = e.second.worldPos(w, h);
        joints.push_back(j);
    }

    // Física de resortes + empuje en Z por las articulaciones cercanas.
    const float rad2 = influenceRadius_ * influenceRadius_;
    for (auto& n : nodes_) {
        // Empuje: buscar la articulación más cercana en (X, Y) y tirar de Z hacia su worldZ.
        float bestDist2 = rad2;
        float targetZ = n.restPos.z;
        for (const auto& j : joints) {
            float dx = j.pos.x - n.pos.x;
            float dy = j.pos.y - n.pos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 < bestDist2) {
                bestDist2 = d2;
                targetZ = j.pos.z;
            }
        }
        float falloff = 1.0f - (bestDist2 / rad2);   // 1 = justo encima, 0 = borde
        if (falloff > 0.0f) {
            // Fuerza hacia el Z objetivo (proporcional a la proximidad en XY).
            n.vel.z += (targetZ - n.pos.z) * 0.25f * falloff;
        }

        // Resorte de reposo: tiende a volver a restPos.
        n.vel += (n.restPos - n.pos) * springK_;

        // Integración con fricción.
        n.vel *= 0.9f;
        n.pos += n.vel * dt * 60.0f;
    }

    // Reconstruir la malla de triángulos con color por desviación en Z.
    mesh_.clear();
    mesh_.setMode(OF_PRIMITIVE_TRIANGLES);
    for (int r = 0; r < rows_; r++) {
        for (int c = 0; c < cols_; c++) {
            const MeshNode& n = nodes_[index(c, r)];
            float deviation = std::abs(n.pos.z - n.restPos.z);
            float t = ofClamp(deviation / 300.0f, 0.0f, 1.0f);
            ofColor col;
            if (t < 0.5f) col = cDim().getLerped(cNeon(), t * 2.0f);
            else          col = cNeon().getLerped(cCore(), (t - 0.5f) * 2.0f);
            mesh_.addVertex(n.pos);
            mesh_.addColor(col);
        }
    }
    for (int r = 0; r < rows_ - 1; r++) {
        for (int c = 0; c < cols_ - 1; c++) {
            int a = index(c, r);
            int b = index(c + 1, r);
            int d = index(c, r + 1);
            int e = index(c + 1, r + 1);
            mesh_.addIndex(a); mesh_.addIndex(b); mesh_.addIndex(d);
            mesh_.addIndex(b); mesh_.addIndex(e); mesh_.addIndex(d);
        }
    }
}

void ElasticDepthMeshScene::draw() {
    if (!isActive_) return;

    const float w = ofGetWidth();
    const float h = ofGetHeight();
    const float now = ofGetElapsedTimef();

    ofEnableAlphaBlending();
    ofEnableDepthTest();

    beginGenesisCamera(cam_, w, h);

    mesh_.draw();

    auto it = interactionPoints_.find("spine_mid");
    if (it != interactionPoints_.end() && it->second.active) {
        drawCoreMotif(it->second.worldPos(w, h), now);
    }

    cam_.end();

    ofDisableDepthTest();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string ElasticDepthMeshScene::getId() const { return "elasticDepthMesh"; }
std::string ElasticDepthMeshScene::getName() const { return "Elastic Depth Mesh"; }
std::vector<std::string> ElasticDepthMeshScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> ElasticDepthMeshScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void ElasticDepthMeshScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> ElasticDepthMeshScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool ElasticDepthMeshScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool ElasticDepthMeshScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool ElasticDepthMeshScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> ElasticDepthMeshScene::getSuggestedMappings() const { return {}; }
Preset ElasticDepthMeshScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata ElasticDepthMeshScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Fase II (Tensión): membrana elástica 2D deformada en Z por la profundidad del cuerpo.";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"3d", "membrane", "spring", "depth", "volumetric"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = false; m.estimatedDuration = 0;
    return m;
}
void ElasticDepthMeshScene::onBeat(int) {} void ElasticDepthMeshScene::onBar(int) {}
bool ElasticDepthMeshScene::isValid() const { return true; }
std::string ElasticDepthMeshScene::getInvalidReason() const { return ""; }
void ElasticDepthMeshScene::unloadResources() { mesh_.clear(); nodes_.clear(); }
void ElasticDepthMeshScene::reloadResources() { setup(); }
void ElasticDepthMeshScene::keyPressed(int) {} void ElasticDepthMeshScene::keyReleased(int) {}
void ElasticDepthMeshScene::mousePressed(int, int, int) {} void ElasticDepthMeshScene::mouseReleased(int, int, int) {}
void ElasticDepthMeshScene::mouseDragged(int, int, int) {} void ElasticDepthMeshScene::mouseMoved(int, int) {}
void ElasticDepthMeshScene::windowResized(int, int) {}

ScenePtr CreateElasticDepthMeshScene() { return std::make_shared<ElasticDepthMeshScene>(); }

} // namespace modular
