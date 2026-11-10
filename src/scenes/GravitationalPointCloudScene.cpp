#include "scenes/GravitationalPointCloudScene.h"
#include "mapping/ParameterTypes.h"
#include <algorithm>

namespace modular {

GravitationalPointCloudScene::GravitationalPointCloudScene() {}
GravitationalPointCloudScene::~GravitationalPointCloudScene() {}

void GravitationalPointCloudScene::buildCloud() {
    stars_.clear();
    stars_.resize(numStars_);
    for (auto& s : stars_) {
        s.basePos = ofVec3f(
            ofRandom(-kWorldHalfW, kWorldHalfW),
            ofRandom(-kWorldHalfH, kWorldHalfH),
            ofRandom(kWorldFarZ, kWorldNearZ)
        );
        s.currPos = s.basePos;
    }
}

void GravitationalPointCloudScene::setup() {
    ofEnableAlphaBlending();
    ofSetBackgroundAuto(true);
    ofBackground(0);
    buildCloud();
}

void GravitationalPointCloudScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    ofSetWindowTitle("Gravitational Point Cloud");
    ofSetFrameRate(60);
    ofBackground(0);
}

void GravitationalPointCloudScene::deactivate() {
    isActive_ = false;
    interactionPoints_.clear();
}

void GravitationalPointCloudScene::pause() { isPaused_ = true; }
void GravitationalPointCloudScene::resume() { isPaused_ = false; }

void GravitationalPointCloudScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.0f, 0.1f);
    for (auto& e : interactionPoints_) e.second.update(dt);

    const float w = ofGetWidth();
    const float h = ofGetHeight();

    // Centro de gravedad = pecho (SPINE_MID), fallback al centro del mundo.
    ofVec3f chest(0.0f, 0.0f, 0.0f);
    bool hasChest = false;
    auto it = interactionPoints_.find("spine_mid");
    if (it != interactionPoints_.end() && it->second.active) {
        chest = it->second.worldPos(w, h);
        hasChest = true;
    }

    // Atracción gravitacional: curva los puntos hacia el pecho.
    const float rad2 = gravityRadius_ * gravityRadius_;
    for (auto& s : stars_) {
        ofVec3f delta = chest - s.basePos;
        float d2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        if (hasChest && d2 < rad2) {
            float d = std::sqrt(d2);
            float falloff = 1.0f - (d / gravityRadius_);
            falloff = falloff * falloff;  // caída cuadrática
            s.currPos = s.basePos + (delta / std::max(d, 0.001f)) * falloff * gravityStrength_ * gravityRadius_;
        } else {
            s.currPos = s.basePos;
        }
    }

    // Reconstruir malla de puntos con color por deformación.
    starMesh_.clear();
    starMesh_.setMode(OF_PRIMITIVE_POINTS);
    for (const auto& s : stars_) {
        float deform = ofClamp((s.currPos - s.basePos).length() / (gravityRadius_ * gravityStrength_), 0.0f, 1.0f);
        ofColor col = cDim().getLerped(cCore(), deform);
        starMesh_.addVertex(s.currPos);
        starMesh_.addColor(col);
    }
}

void GravitationalPointCloudScene::draw() {
    if (!isActive_) return;

    const float w = ofGetWidth();
    const float h = ofGetHeight();
    const float now = ofGetElapsedTimef();

    ofEnableAlphaBlending();
    ofEnableDepthTest();

    beginGenesisCamera(cam_, w, h);

    glPointSize(5.0f);
    starMesh_.draw();
    glPointSize(1.0f);

    auto it = interactionPoints_.find("spine_mid");
    if (it != interactionPoints_.end() && it->second.active) {
        drawCoreMotif(it->second.worldPos(w, h), now);
    }

    cam_.end();

    ofDisableDepthTest();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string GravitationalPointCloudScene::getId() const { return "gravitationalPointCloud"; }
std::string GravitationalPointCloudScene::getName() const { return "Gravitational Point Cloud"; }
std::vector<std::string> GravitationalPointCloudScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> GravitationalPointCloudScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void GravitationalPointCloudScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> GravitationalPointCloudScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool GravitationalPointCloudScene::setParameter(const std::string& id, float v) {
    (void)id; (void)v; return false;
}
bool GravitationalPointCloudScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool GravitationalPointCloudScene::setParameter(const std::string& id, const glm::vec3& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id];
    pt.sourceName = id;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(v.x, v.y), 1.0f, ofGetWidth(), ofGetHeight(), v.z);
    return true;
}
std::vector<SuggestedMapping> GravitationalPointCloudScene::getSuggestedMappings() const { return {}; }
Preset GravitationalPointCloudScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    return pr;
}
SceneMetadata GravitationalPointCloudScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Fase III (Disolución): polvo estelar — nube de puntos deformada por la gravedad del cuerpo en Z.";
    m.author = "Génesis Volumétrica"; m.version = "1.0.0";
    m.tags = {"3d", "pointcloud", "gravity", "volumetric", "genesis"}; m.category = "volumetric";
    m.hasInternalState = true; m.isTimeBased = false; m.estimatedDuration = 0;
    return m;
}
void GravitationalPointCloudScene::onBeat(int) {} void GravitationalPointCloudScene::onBar(int) {}
bool GravitationalPointCloudScene::isValid() const { return true; }
std::string GravitationalPointCloudScene::getInvalidReason() const { return ""; }
void GravitationalPointCloudScene::unloadResources() { starMesh_.clear(); stars_.clear(); }
void GravitationalPointCloudScene::reloadResources() { setup(); }
void GravitationalPointCloudScene::keyPressed(int) {} void GravitationalPointCloudScene::keyReleased(int) {}
void GravitationalPointCloudScene::mousePressed(int, int, int) {} void GravitationalPointCloudScene::mouseReleased(int, int, int) {}
void GravitationalPointCloudScene::mouseDragged(int, int, int) {} void GravitationalPointCloudScene::mouseMoved(int, int) {}
void GravitationalPointCloudScene::windowResized(int, int) {}

ScenePtr CreateGravitationalPointCloudScene() { return std::make_shared<GravitationalPointCloudScene>(); }

} // namespace modular
