#include "scenes/GridDissolveScene.h"
#include "registry/ParameterRegistry.h"
#include "mapping/ParameterTypes.h"
#include "ofMain.h"
#include <glm/glm.hpp>
#include <algorithm>

namespace modular {

void GridDissolveScene::setupPalettes() {
    if (!palettes_.empty()) return;
    palettes_ = {
        {{1,0,1},{1,.08f,.58f},{1,0,.5f},{1,.41f,.71f},{1,0,.8f}},       // magenta
        {{0,1,1},{0,.81f,.82f},{.25f,.88f,.82f},{0,.75f,1},{.02f,.85f,1}}, // cyan
        {{0,1,0},{.22f,1,.08f},{.2f,.8f,.2f},{0,.98f,.6f},{.49f,.99f,0}},   // green
        {{1,.42f,0},{1,.27f,0},{1,.55f,0},{1,.5f,.31f},{1,.39f,.28f}},       // orange
        {{1,1,0},{1,.84f,0},{.8f,1,0},{1,1,.2f},{.9f,1,0}},                 // yellow
    };
}

ofFloatColor GridDissolveScene::randomColor() {
    auto& p = palettes_[currentPalette_ % palettes_.size()];
    return p[(int)ofRandom(p.size())];
}

GridDissolveScene::GridDissolveScene() { initParams(); }
GridDissolveScene::~GridDissolveScene() { unregisterParams(); }

void GridDissolveScene::initParams() {
    if (!params_.empty()) return;
    auto add = [&](auto& k, auto& l, float lo, float hi, float d, float st, auto& desc, ParameterType t = ParameterType::FLOAT) {
        auto p = std::make_shared<Parameter>();
        p->id = getId() + "." + k; p->label = l; p->type = t; p->scope = ParameterScope::SCENE;
        p->min = lo; p->max = hi; p->defaultValue = d; p->currentValue = d;
        p->step = st; p->group = getId(); p->description = desc;
        p->hidden = false; p->writable = true;
        params_[k] = p;
    };
    add("cols", "Columns", 5, 40, (float)cols_, 1, "Grid columns", ParameterType::INT);
    add("rows", "Rows", 5, 30, (float)rows_, 1, "Grid rows", ParameterType::INT);
    add("dissolveDuration", "Dissolve", 0.2f, 3.0f, dissolveDuration_, 0.1f, "Dissolve seconds");
    add("touchRadius", "Touch Radius", 2, 40, touchRadius_, 1, "Joint hit radius px");
}
void GridDissolveScene::registerParams() {
    if (paramsReg_) return; initParams();
    auto& r = ParameterRegistry::getGlobal();
    for (auto& e : params_) if (e.second) r.registerParam(*e.second);
    paramsReg_ = true;
}
void GridDissolveScene::unregisterParams() {
    if (!paramsReg_) return;
    auto& r = ParameterRegistry::getGlobal();
    for (auto& e : params_) if (e.second) r.unregisterParam(e.second->id);
    paramsReg_ = false;
}
void GridDissolveScene::syncParams() {
    auto ri = [&](auto& k, int& t) { auto it = params_.find(k); if (it != params_.end() && it->second) t = (int)it->second->currentValue; };
    auto rf = [&](auto& k, float& t) { auto it = params_.find(k); if (it != params_.end() && it->second) t = it->second->currentValue; };
    ri("cols", cols_); ri("rows", rows_);
    rf("dissolveDuration", dissolveDuration_); rf("touchRadius", touchRadius_);
}

void GridDissolveScene::setup() {
    ofEnableAlphaBlending(); setupPalettes(); initParams();
    ofSetBackgroundAuto(true); ofBackground(5);
}
void GridDissolveScene::activate() {
    isActive_ = true; isPaused_ = false;
    ofSetWindowTitle("Grid Dissolve"); ofSetFrameRate(60);
    ofBackground(5);
    auto& r = ParameterRegistry::getGlobal();
    if (auto pp = r.getParam("global.currentPalette")) currentPalette_ = (int)pp->currentValue % 5;
    registerParams();
    auto px = getId() + ".";
    for (auto& e : params_) { auto f = px + e.first; if (auto p = r.getParam(f)) setParameter(e.first, p->currentValue); }
    syncParams();
    buildGrid();
}
void GridDissolveScene::deactivate() {
    isActive_ = false; unregisterParams();
    interactionPoints_.clear(); cells_.clear();
}
void GridDissolveScene::pause() { isPaused_ = true; }
void GridDissolveScene::resume() { isPaused_ = false; }

void GridDissolveScene::buildGrid() {
    cells_.clear();
    float cw = (float)ofGetWidth() / cols_;
    float rh = (float)ofGetHeight() / rows_;
    for (int r = 0; r < rows_; r++) {
        for (int c = 0; c < cols_; c++) {
            DissolveCell cell;
            cell.rect = ofRectangle(c * cw, r * rh, cw, rh);
            cell.color = randomColor();
            cell.alive = true;
            cells_.push_back(cell);
        }
    }
}

void GridDissolveScene::update(float dt) {
    if (!isActive_ || isPaused_) return;
    dt = glm::clamp(dt, 0.f, 0.1f);
    for (auto& e : interactionPoints_) e.second.update(dt);

    // Check joints against living cells
    for (auto& cell : cells_) {
        if (!cell.alive) continue;  // already dead

        for (auto& e : interactionPoints_) {
            if (!e.second.active) continue;
            if (cell.rect.inside(e.second.position.x, e.second.position.y)) {
                // Start dissolve
                cell.alive = false;
                cell.dissolveTime = 0.0f;

                // Generate irregular polygonal shards (6-10 fragments)
                int numShards = 6 + (int)ofRandom(5);
                cell.shards.resize(numShards);
                float cx = cell.rect.x + cell.rect.width * 0.5f;
                float cy = cell.rect.y + cell.rect.height * 0.5f;

                for (int s = 0; s < numShards; s++) {
                    auto& shard = cell.shards[s];
                    // Random angle from center + random distance on each edge
                    float baseAngle = (float)s / numShards * glm::two_pi<float>() + ofRandom(-0.3f, 0.3f);
                    float nextAngle = (float)(s + 1) / numShards * glm::two_pi<float>() + ofRandom(-0.3f, 0.3f);

                    // Intersect rays with rectangle to get edge points
                    auto rayToEdge = [&](float angle) -> glm::vec2 {
                        glm::vec2 dir(std::cos(angle), std::sin(angle));
                        float tx = (dir.x > 0 ? cell.rect.x + cell.rect.width - cx : cell.rect.x - cx) / (dir.x != 0 ? dir.x : 0.0001f);
                        float ty = (dir.y > 0 ? cell.rect.y + cell.rect.height - cy : cell.rect.y - cy) / (dir.y != 0 ? dir.y : 0.0001f);
                        float t = std::min(std::max(tx, 0.0f), std::max(ty, 0.0f));
                        return glm::vec2(cx + dir.x * t, cy + dir.y * t);
                    };

                    glm::vec2 p1 = rayToEdge(baseAngle);
                    glm::vec2 p2 = rayToEdge(nextAngle);
                    // Slightly inset from edges for visual gap
                    glm::vec2 mid = (p1 + p2) * 0.5f;

                    shard.vertices = {
                        glm::vec2(cx, cy) - glm::vec2(cx, cy) + (mid * 0.15f),  // near center
                        p1 - glm::vec2(cx, cy),
                        p2 - glm::vec2(cx, cy),
                    };
                    // Add jitter to vertices
                    for (auto& v : shard.vertices) {
                        v += glm::vec2(ofRandom(-4, 4), ofRandom(-4, 4));
                    }
                    shard.offset = glm::vec2(cx, cy);
                    shard.alpha = 1.0f;
                    shard.angle = ofRandom(0, glm::two_pi<float>());
                    shard.angVel = ofRandom(-3.0f, 3.0f);
                }
                break;  // one joint per cell per frame
            }
        }
    }

    // Animate dissolving cells
    for (auto& cell : cells_) {
        if (cell.alive) continue;
        if (cell.shards.empty()) continue;
        cell.dissolveTime += dt;
        float t = cell.dissolveTime / dissolveDuration_;
        if (t >= 1.0f) {
            cell.shards.clear();
            continue;
        }
        // Shards fly outward from center, rotate, and fade
        float spread = t * 80.0f;
        for (auto& shard : cell.shards) {
            // Move outward from cell center
            glm::vec2 dir = glm::normalize(shard.offset - glm::vec2(
                cell.rect.x + cell.rect.width * 0.5f,
                cell.rect.y + cell.rect.height * 0.5f));
            if (glm::length(dir) < 0.001f) dir = glm::vec2(ofRandom(-1, 1), ofRandom(-1, 1));
            shard.offset += dir * spread * dt * 2.0f;
            shard.angle += shard.angVel * dt;
            shard.alpha = 1.0f - t * t;  // ease-out fade
        }
    }
}

void GridDissolveScene::draw() {
    if (!isActive_) return;
    ofPushStyle();
    ofEnableAlphaBlending();

    // Depth relation: body proximity scales cell brightness.
    float proximity = averageProximity(interactionPoints_);
    float cellAlpha = ofClamp(120.0f + 140.0f * proximity, 40.0f, 255.0f);

    // Draw living cells
    for (auto& cell : cells_) {
        if (!cell.alive) continue;
        ofSetColor(cell.color.r * 255, cell.color.g * 255, cell.color.b * 255, cellAlpha);
        ofFill();
        ofDrawRectangle(cell.rect);
        // Subtle border
        ofNoFill();
        ofSetColor(255, 255, 255, 30);
        ofSetLineWidth(0.5f);
        ofDrawRectangle(cell.rect);
    }

    // Draw dissolving polygonal shards
    ofFill();
    for (auto& cell : cells_) {
        if (cell.alive || cell.shards.empty()) continue;
        for (auto& shard : cell.shards) {
            float a = shard.alpha * 200;
            if (a <= 0) continue;
            ofSetColor(cell.color.r * 255, cell.color.g * 255, cell.color.b * 255, a);
            ofPushMatrix();
            ofTranslate(shard.offset.x, shard.offset.y);
            ofRotateRad(shard.angle);
            ofBeginShape();
            for (auto& v : shard.vertices) ofVertex(v.x, v.y);
            ofEndShape(true);
            ofPopMatrix();
        }
    }

    ofPopStyle();
}

// ── boilerplate ───────────────────────────────────────────────────────────
std::string GridDissolveScene::getId() const { return "gridDissolve"; }
std::string GridDissolveScene::getName() const { return "Grid Dissolve"; }
std::vector<std::string> GridDissolveScene::getParameterIds() const {
    std::vector<std::string> ids; for (auto& e : params_) ids.push_back(e.first); return ids;
}
std::vector<std::string> GridDissolveScene::getInteractionPointIds() const {
    std::vector<std::string> ids; for (auto& e : interactionPoints_) ids.push_back(e.first); return ids;
}
void GridDissolveScene::clearInteractionPoints() { interactionPoints_.clear(); }
std::shared_ptr<Parameter> GridDissolveScene::getParameter(const std::string& id) {
    auto it = params_.find(id); return it != params_.end() ? it->second : nullptr;
}
bool GridDissolveScene::setParameter(const std::string& id, float v) {
    auto key = id; auto pfx = getId() + "."; if (key.find(pfx) == 0) key = key.substr(pfx.size());
    auto it = params_.find(key); if (it == params_.end() || !it->second) return false;
    it->second->currentValue = v; syncParams(); return true;
}
bool GridDissolveScene::setParameter(const std::string& id, const glm::vec2& v) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[id]; pt.sourceName = id; pt.decayRate = 0.08f;
    pt.setTarget(v, 1.0f, ofGetWidth(), ofGetHeight()); return true;
}
bool GridDissolveScene::setParameter(const std::string& paramId, const glm::vec3& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(value.x, value.y), 1.0f, ofGetWidth(), ofGetHeight(), value.z);
    return true;
}
std::vector<SuggestedMapping> GridDissolveScene::getSuggestedMappings() const { return {}; }
Preset GridDissolveScene::getDefaultPreset() const {
    Preset pr; pr.id = "default"; pr.name = "Default"; pr.sceneId = getId(); pr.isDefault = true;
    for (auto& e : params_) { Parameter cp = *e.second; cp.currentValue = cp.defaultValue; pr.parameters.push_back(cp); }
    return pr;
}
SceneMetadata GridDissolveScene::getSceneMetadata() const {
    SceneMetadata m; m.id = getId(); m.name = getName();
    m.description = "Colored grid dissolves when skeleton joints touch it. Cells split into 4 fragments that shrink and fade.";
    m.author = "Proyecto Danza"; m.version = "1.0.0";
    m.tags = {"grid", "dissolve", "interactive", "joints", "color"}; m.category = "visual";
    m.hasInternalState = true; m.isTimeBased = false; m.estimatedDuration = 0;
    return m;
}
void GridDissolveScene::onBeat(int) {} void GridDissolveScene::onBar(int) {}
bool GridDissolveScene::isValid() const { return true; }
std::string GridDissolveScene::getInvalidReason() const { return ""; }
void GridDissolveScene::unloadResources() { cells_.clear(); }
void GridDissolveScene::reloadResources() { setup(); }
void GridDissolveScene::keyPressed(int) {} void GridDissolveScene::keyReleased(int) {}
void GridDissolveScene::mousePressed(int, int, int) {} void GridDissolveScene::mouseReleased(int, int, int) {}
void GridDissolveScene::mouseDragged(int, int, int) {} void GridDissolveScene::mouseMoved(int, int) {}
void GridDissolveScene::windowResized(int, int) { buildGrid(); }

ScenePtr CreateGridDissolveScene() { return std::make_shared<GridDissolveScene>(); }

} // namespace modular
