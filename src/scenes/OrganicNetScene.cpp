#include "scenes/OrganicNetScene.h"
#include "registry/ParameterRegistry.h"
#include <cmath>
#include <algorithm>

namespace modular {

// ============================================================================
// Constructor / Destructor
// ============================================================================

OrganicNetScene::OrganicNetScene()
    : currentPalette_(0)
    , isActive_(false)
    , isPaused_(false)
    , isValid_(true)
    , invalidReason_("")
    , parametersRegistered_(false)
    , debugMarkers_(false) {
}

OrganicNetScene::~OrganicNetScene() {
    unregisterParameters();
}

// ============================================================================
// Palette System
// ============================================================================

void OrganicNetScene::setupPalettes() {
    palettes_.push_back({
        ofFloatColor::fromHex(0xFF00FF),
        ofFloatColor::fromHex(0xFF1493),
        ofFloatColor::fromHex(0xFF0080),
        ofFloatColor::fromHex(0xFF69B4),
        ofFloatColor::fromHex(0xFF00CC)
    });
    palettes_.push_back({
        ofFloatColor::fromHex(0x00FFFF),
        ofFloatColor::fromHex(0x00CED1),
        ofFloatColor::fromHex(0x40E0D0),
        ofFloatColor::fromHex(0x00BFFF),
        ofFloatColor::fromHex(0x04D9FF)
    });
    palettes_.push_back({
        ofFloatColor::fromHex(0x00FF00),
        ofFloatColor::fromHex(0x39FF14),
        ofFloatColor::fromHex(0x32CD32),
        ofFloatColor::fromHex(0x00FA9A),
        ofFloatColor::fromHex(0x7CFC00)
    });
    palettes_.push_back({
        ofFloatColor::fromHex(0xFF6B00),
        ofFloatColor::fromHex(0xFF4500),
        ofFloatColor::fromHex(0xFF8C00),
        ofFloatColor::fromHex(0xFF7F50),
        ofFloatColor::fromHex(0xFF6347)
    });
    palettes_.push_back({
        ofFloatColor::fromHex(0xFFFF00),
        ofFloatColor::fromHex(0xFFD700),
        ofFloatColor::fromHex(0xCCFF00),
        ofFloatColor::fromHex(0xFFFF33),
        ofFloatColor::fromHex(0xE5FF00)
    });
}

// ============================================================================
// Net Construction
// ============================================================================

void OrganicNetScene::buildNet() {
    nodes_.clear();
    edges_.clear();

    int R = netParams_.rows;
    int C = netParams_.cols;
    float S = netParams_.spacing;

    float totalW = (C - 1) * S;
    float totalH = (R - 1) * S;
    float startX = (ofGetWidth() - totalW) * 0.5f;
    float startY = ofGetHeight() * 0.15f;  // top area

    nodeRows_ = R;
    nodeCols_ = C;
    nodes_.reserve(R * C);

    // Create nodes with random offsets from perfect grid
    for (int r = 0; r < R; r++) {
        for (int c = 0; c < C; c++) {
            NetNode node;
            float baseX = startX + c * S;
            float baseY = startY + r * S;
            node.originalPos = glm::vec2(
                baseX + ofRandom(-netParams_.randomOffsetX, netParams_.randomOffsetX),
                baseY + ofRandom(-netParams_.randomOffsetY, netParams_.randomOffsetY)
            );
            node.position = node.originalPos;
            node.velocity = glm::vec2(0.0f);
            node.noiseOffset = ofRandom(0.0f, 1000.0f);
            nodes_.push_back(node);
        }
    }

    auto idx = [C](int r, int c) -> int { return r * C + c; };

    // Horizontal connections
    for (int r = 0; r < R; r++) {
        for (int c = 0; c < C - 1; c++) {
            if (ofRandomuf() < netParams_.omitChance) continue;
            int a = idx(r, c);
            int b = idx(r, c + 1);
            float dist = glm::distance(nodes_[a].originalPos, nodes_[b].originalPos);
            edges_.push_back({a, b, dist});
        }
    }

    // Vertical connections
    for (int r = 0; r < R - 1; r++) {
        for (int c = 0; c < C; c++) {
            if (ofRandomuf() < netParams_.omitChance) continue;
            int a = idx(r, c);
            int b = idx(r + 1, c);
            float dist = glm::distance(nodes_[a].originalPos, nodes_[b].originalPos);
            edges_.push_back({a, b, dist});
        }
    }

    // Diagonal connections (main diagonal: ↘)
    for (int r = 0; r < R - 1; r++) {
        for (int c = 0; c < C - 1; c++) {
            if (ofRandomuf() < netParams_.omitChance + 0.15f) continue;  // more omissions for diagonals
            int a = idx(r, c);
            int b = idx(r + 1, c + 1);
            float dist = glm::distance(nodes_[a].originalPos, nodes_[b].originalPos);
            edges_.push_back({a, b, dist});
        }
    }

    // Anti-diagonal connections (↗)
    for (int r = 0; r < R - 1; r++) {
        for (int c = 1; c < C; c++) {
            if (ofRandomuf() < netParams_.omitChance + 0.15f) continue;
            int a = idx(r, c);
            int b = idx(r + 1, c - 1);
            float dist = glm::distance(nodes_[a].originalPos, nodes_[b].originalPos);
            edges_.push_back({a, b, dist});
        }
    }

    ofLogNotice("OrganicNetScene") << "Built net: " << nodes_.size()
        << " nodes, " << edges_.size() << " edges";
}

void OrganicNetScene::resetNet() {
    for (auto& node : nodes_) {
        node.position = node.originalPos;
        node.velocity = glm::vec2(0.0f);
    }
}

// ============================================================================
// Spring Constraints (relaxation iterations)
// ============================================================================

void OrganicNetScene::applyConstraints() {
    float stiffness = netParams_.springStiffness;

    for (int iter = 0; iter < netParams_.springIterations; iter++) {
        for (const auto& edge : edges_) {
            NetNode& a = nodes_[edge.a];
            NetNode& b = nodes_[edge.b];

            glm::vec2 delta = b.position - a.position;
            float dist = glm::length(delta);
            if (dist < 0.001f) continue;

            float correction = (dist - edge.restLength) * stiffness;
            glm::vec2 dir = delta / dist;

            a.position += dir * correction * 0.5f;
            b.position -= dir * correction * 0.5f;

            // Small velocity correction for stability
            a.velocity += dir * correction * 0.2f;
            b.velocity -= dir * correction * 0.2f;
        }
    }
}

// ============================================================================
// Organic Line Drawing
// ============================================================================

void OrganicNetScene::drawOrganicLine(const glm::vec2& a, const glm::vec2& b,
                                       float noiseBase) {
    float time = ofGetElapsedTimef();
    int segs = netParams_.lineSegments;
    float nsScale = netParams_.lineNoiseScale;
    float nsAmount = netParams_.lineNoiseAmount;

    glm::vec2 dir = b - a;
    float len = glm::length(dir);
    if (len < 0.001f) return;

    glm::vec2 dirNorm = dir / len;
    glm::vec2 perp(-dirNorm.y, dirNorm.x);

    ofBeginShape();
    for (int i = 0; i <= segs; i++) {
        float t = static_cast<float>(i) / static_cast<float>(segs);
        glm::vec2 base = a + dir * t;

        // Multi-octave noise displacement perpendicular to line direction
        float nx = (base.x * nsScale);
        float ny = (base.y * nsScale);
        float n1 = ofSignedNoise(nx + time * 0.5f + noiseBase);
        float n2 = ofSignedNoise(ny + time * 0.7f + noiseBase * 1.3f) * 0.6f;
        float n3 = ofSignedNoise(nx * 1.7f + ny * 0.3f + time * 0.3f) * 0.35f;
        float noiseVal = n1 * 0.6f + n2 + n3;

        float offset = noiseVal * nsAmount;
        // Taper offset near endpoints (so lines connect cleanly at nodes)
        float taper = 1.0f - std::abs(t * 2.0f - 1.0f);  // 1 at middle, 0 at ends
        taper = taper * taper;  // sharper taper
        offset *= (0.4f + 0.6f * taper);

        glm::vec2 pt = base + perp * offset;
        ofVertex(pt.x, pt.y);
    }
    ofEndShape(false);  // open stroke
}

// ============================================================================
// Parameter Definitions
// ============================================================================

void OrganicNetScene::initializeParameterDefinitions() {
    if (!parameters_.empty()) return;

    const std::string sceneId = getId();
    auto addParam = [&](const std::string& key,
                        const std::string& label,
                        float minVal, float maxVal, float defaultVal,
                        float stepVal, const std::string& description,
                        ParameterType type = ParameterType::FLOAT) {
        auto param = std::make_shared<Parameter>();
        param->id = sceneId + "." + key;
        param->label = label;
        param->type = type;
        param->scope = ParameterScope::SCENE;
        param->min = minVal;
        param->max = maxVal;
        param->defaultValue = defaultVal;
        param->currentValue = defaultVal;
        param->step = stepVal;
        param->group = sceneId;
        param->description = description;
        param->hidden = false;
        param->writable = true;
        parameters_[key] = param;
    };

    addParam("grid.rows", "Filas",
             6.0f, 24.0f, 12.0f, 1.0f,
             "Cantidad de filas de la red", ParameterType::INT);
    addParam("grid.cols", "Columnas",
             8.0f, 36.0f, 18.0f, 1.0f,
             "Cantidad de columnas de la red", ParameterType::INT);
    addParam("grid.spacing", "Espaciado",
             30.0f, 60.0f, 45.0f, 1.0f,
             "Distancia entre nodos (px)");
    addParam("grid.randomOffset", "Offset Aleatorio",
             0.0f, 30.0f, 15.0f, 1.0f,
             "Desplazamiento aleatorio maximo de nodos");
    addParam("grid.omitChance", "Omitir Conexion",
             0.0f, 0.4f, 0.12f, 0.01f,
             "Probabilidad de omitir una conexion");

    addParam("phys.gravity", "Gravedad",
             0.005f, 0.06f, 0.025f, 0.001f,
             "Gravedad vertical suave");
    addParam("phys.damping", "Amortiguacion",
             0.9f, 0.995f, 0.96f, 0.001f,
             "Friccion general de velocidad");
    addParam("phys.springStiffness", "Rigidez Resortes",
             0.02f, 0.4f, 0.15f, 0.01f,
             "Fuerza de constraints entre nodos");
    addParam("phys.springIterations", "Iteraciones Resorte",
             1.0f, 8.0f, 3.0f, 1.0f,
             "Pasadas de relajacion por frame", ParameterType::INT);

    addParam("noise.strength", "Fuerza Noise",
             0.1f, 2.0f, 0.8f, 0.05f,
             "Intensidad del movimiento organico");
    addParam("noise.scale", "Escala Noise",
             0.005f, 0.04f, 0.015f, 0.001f,
             "Escala espacial del noise");

    addParam("wind.strength", "Fuerza Viento",
             0.0f, 1.5f, 0.4f, 0.05f,
             "Intensidad del viento lateral");
    addParam("wind.frequency", "Frecuencia Viento",
             0.1f, 1.5f, 0.3f, 0.05f,
             "Frecuencia de oscilacion del viento");
    addParam("wind.bias", "Direccion Viento",
             -0.8f, 0.8f, 0.0f, 0.05f,
             "Direccion base del viento (-izq, +der)");

    addParam("lift.radius", "Radio Lift",
             40.0f, 250.0f, 120.0f, 5.0f,
             "Radio de influencia del levantamiento (px)");
    addParam("lift.strength", "Fuerza Lift",
             0.2f, 3.5f, 1.5f, 0.1f,
             "Fuerza de elevacion de joints");

    addParam("return.force", "Fuerza Retorno",
             0.0f, 0.03f, 0.008f, 0.001f,
             "Fuerza de retorno suave a posicion original");

    addParam("line.segments", "Segmentos Linea",
             4.0f, 20.0f, 10.0f, 1.0f,
             "Segmentos por linea organica", ParameterType::INT);
    addParam("line.noiseAmount", "Ondulacion Linea",
             2.0f, 30.0f, 10.0f, 1.0f,
             "Amplitud de ondulacion de lineas (px)");
    addParam("line.alpha", "Alpha Lineas",
             20.0f, 255.0f, 180.0f, 5.0f,
             "Transparencia de las lineas");
    addParam("line.width", "Grosor Linea",
             0.3f, 4.0f, 1.2f, 0.1f,
             "Grosor base de las lineas (px)");
    addParam("node.radius", "Radio Nodos",
             0.5f, 6.0f, 2.5f, 0.5f,
             "Radio de los nodos (px)");
    addParam("node.alpha", "Alpha Nodos",
             0.0f, 255.0f, 100.0f, 5.0f,
             "Transparencia de los nodos");
}

// ============================================================================
// Parameter Registration / Sync
// ============================================================================

void OrganicNetScene::registerParameters() {
    if (parametersRegistered_) return;
    initializeParameterDefinitions();
    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (!param) continue;
        Parameter definition = *param;
        bool registered = registry.registerParam(definition);
        if (!registered && !registry.hasParam(definition.id)) {
            ofLogWarning("OrganicNetScene")
                << "Failed to register parameter " << definition.id;
        }
    }
    parametersRegistered_ = true;
}

void OrganicNetScene::unregisterParameters() {
    if (!parametersRegistered_) return;
    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (param) registry.unregisterParam(param->id);
    }
    parametersRegistered_ = false;
}

void OrganicNetScene::syncParamsFromMap() {
    auto readFloat = [&](const std::string& key, float& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) target = it->second->currentValue;
    };
    auto readInt = [&](const std::string& key, int& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) target = static_cast<int>(it->second->currentValue);
    };

    readInt("grid.rows", netParams_.rows);
    readInt("grid.cols", netParams_.cols);
    readFloat("grid.spacing", netParams_.spacing);
    readFloat("grid.randomOffset", netParams_.randomOffsetX);
    netParams_.randomOffsetY = netParams_.randomOffsetX;
    readFloat("grid.omitChance", netParams_.omitChance);
    readFloat("phys.gravity", netParams_.gravity);
    readFloat("phys.damping", netParams_.damping);
    readFloat("phys.springStiffness", netParams_.springStiffness);
    readInt("phys.springIterations", netParams_.springIterations);
    readFloat("noise.strength", netParams_.noiseStrength);
    readFloat("noise.scale", netParams_.noiseScale);
    readFloat("wind.strength", netParams_.windStrength);
    readFloat("wind.frequency", netParams_.windFrequency);
    readFloat("wind.bias", netParams_.windBias);
    readFloat("lift.radius", netParams_.liftRadius);
    readFloat("lift.strength", netParams_.liftStrength);
    readFloat("return.force", netParams_.returnForce);
    readInt("line.segments", netParams_.lineSegments);
    readFloat("line.noiseAmount", netParams_.lineNoiseAmount);
    readFloat("line.alpha", netParams_.lineAlpha);
    readFloat("line.width", netParams_.lineWidth);
    readFloat("node.radius", netParams_.nodeRadius);
    readFloat("node.alpha", netParams_.nodeAlpha);
}

// ============================================================================
// Lifecycle
// ============================================================================

void OrganicNetScene::setup() {
    ofEnableAlphaBlending();
    setupPalettes();
    initializeParameterDefinitions();
    ofSetBackgroundAuto(true);
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) bg = param->currentValue;
    ofBackground(static_cast<int>(bg));
}

void OrganicNetScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    interactionPoints_.clear();

    ofSetWindowTitle("Organic Net — Red Orgánica Flotante");
    ofSetFrameRate(60);

    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) bg = param->currentValue;
    ofBackground(static_cast<int>(bg));

    if (auto paletteParam = registry.getParam("global.currentPalette"))
        currentPalette_ = static_cast<int>(paletteParam->currentValue);

    registerParameters();

    const std::string ownPrefix = getId() + ".";
    for (const auto& entry : parameters_) {
        const std::string& key = entry.first;
        std::string fullId = ownPrefix + key;
        if (auto param = registry.getParam(fullId)) {
            setParameter(key, param->currentValue);
        }
    }

    syncParamsFromMap();
    buildNet();
}

void OrganicNetScene::deactivate() {
    isActive_ = false;
    unregisterParameters();
    interactionPoints_.clear();
    nodes_.clear();
    edges_.clear();
}

void OrganicNetScene::pause() { isPaused_ = true; }
void OrganicNetScene::resume() { isPaused_ = false; }

std::string OrganicNetScene::getId() const { return "organicNet"; }
std::string OrganicNetScene::getName() const { return "Organic Net — Red Orgánica"; }

// ============================================================================
// Update
// ============================================================================

void OrganicNetScene::update(float deltaTime) {
    if (!isActive_ || isPaused_ || nodes_.empty()) return;

    // 1. Update interaction points
    for (auto& entry : interactionPoints_) {
        entry.second.update(deltaTime);
    }

    float dt60 = deltaTime * 60.0f;
    float time = ofGetElapsedTimef();

    // 2. Physics per node
    for (auto& node : nodes_) {
        // Gravity (gentle)
        node.velocity.y += netParams_.gravity * dt60;

        // Wind
        float windWave = std::sin(time * netParams_.windFrequency + node.noiseOffset * 0.1f);
        float wind = (netParams_.windBias + windWave * netParams_.windStrength) * dt60;
        node.velocity.x += wind;

        // Organic noise drift
        float nx = node.position.x * netParams_.noiseScale;
        float ny = node.position.y * netParams_.noiseScale;
        float driftX = ofSignedNoise(nx + time * 0.4f, ny + node.noiseOffset * 0.01f)
                     * netParams_.noiseStrength * dt60;
        float driftY = ofSignedNoise(ny + time * 0.5f, nx - node.noiseOffset * 0.01f)
                     * netParams_.noiseStrength * dt60 * 0.7f;
        node.velocity.x += driftX;
        node.velocity.y += driftY;

        // Damping
        node.velocity *= netParams_.damping;

        // Integrate
        node.position += node.velocity * dt60;
    }

    // 3. Joint lift: joints push the net upward
    for (const auto& ipEntry : interactionPoints_) {
        const InteractionPoint& ip = ipEntry.second;
        if (!ip.active) continue;

        glm::vec2 jointPos = ip.position;

        for (auto& node : nodes_) {
            float dx = node.position.x - jointPos.x;
            float dy = node.position.y - jointPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist > 0.001f && dist < netParams_.liftRadius) {
                // Quadratic falloff: stronger near the joint
                float t = 1.0f - dist / netParams_.liftRadius;
                float strength = t * t * netParams_.liftStrength * dt60;

                // Main lift: upward
                node.velocity.y -= strength;

                // Lateral spread: push outward from joint center
                float lateral = strength * 0.35f;
                node.velocity.x += (dx / dist) * lateral;
            }
        }
    }

    // 4. Spring constraints
    applyConstraints();

    // 5. Gentle return to original Y (prevents falling off screen forever)
    for (auto& node : nodes_) {
        float dy = node.originalPos.y - node.position.y;
        if (dy > 50.0f) {  // only pull up if fallen significantly
            node.velocity.y += dy * netParams_.returnForce * dt60 * 0.1f;
        }
        // Soft horizontal return (much weaker)
        float dx = node.originalPos.x - node.position.x;
        node.velocity.x += dx * netParams_.returnForce * 0.15f * dt60 * 0.1f;
    }
}

// ============================================================================
// Draw
// ============================================================================

void OrganicNetScene::draw() {
    if (!isActive_ || nodes_.empty()) return;

    ofEnableAlphaBlending();
    ofNoFill();

    const auto& palette = palettes_[currentPalette_ % palettes_.size()];
    ofFloatColor lineColor = palette[0];  // head palette color for lines
    float lineA = netParams_.lineAlpha;

    float time = ofGetElapsedTimef();

    // Depth relation: body proximity scales line width and node size.
    float depthScale = 0.7f + 0.6f * averageProximity(interactionPoints_);

    // Draw organic lines for each edge
    for (size_t e = 0; e < edges_.size(); e++) {
        const NetEdge& edge = edges_[e];
        if (edge.a < 0 || edge.b < 0) continue;
        if (edge.a >= (int)nodes_.size() || edge.b >= (int)nodes_.size()) continue;

        const NetNode& a = nodes_[edge.a];
        const NetNode& b = nodes_[edge.b];

        // Distance-based alpha: closer nodes = more opaque
        float dist = glm::distance(a.position, b.position);
        float stretchRatio = (edge.restLength > 0.001f)
            ? dist / edge.restLength : 1.0f;
        // Lines at rest length = full alpha, stretched = more transparent
        float distAlpha = 1.0f - glm::clamp(std::abs(stretchRatio - 1.0f) * 2.0f, 0.0f, 0.7f);

        int alphaByte = static_cast<int>(lineA * distAlpha);
        ofSetColor(lineColor.r * 255.0f, lineColor.g * 255.0f, lineColor.b * 255.0f, alphaByte);
        ofSetLineWidth(netParams_.lineWidth * depthScale);

        drawOrganicLine(a.position, b.position, a.noiseOffset + e * 0.3f);
    }

    // Draw subtle nodes
    ofFill();
    for (size_t i = 0; i < nodes_.size(); i++) {
        const NetNode& node = nodes_[i];
        float nodeA = netParams_.nodeAlpha;
        int aByte = static_cast<int>(nodeA);
        ofSetColor(lineColor.r * 255.0f, lineColor.g * 255.0f, lineColor.b * 255.0f, aByte);
        float r = netParams_.nodeRadius * depthScale;
        ofDrawCircle(node.position.x, node.position.y, r);
    }

    // Joint markers (debug)
    if (debugMarkers_) {
        ofFill();
        ofSetColor(255, 0, 0, 150);
        for (const auto& entry : interactionPoints_) {
            if (entry.second.active) {
                float mr = 8.0f;
                ofSetLineWidth(2.0f);
                ofDrawCircle(entry.second.position.x, entry.second.position.y, mr);
            }
        }
    }
}

// ============================================================================
// Parameter Access
// ============================================================================

std::vector<std::string> OrganicNetScene::getParameterIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : parameters_) ids.push_back(pair.first);
    return ids;
}

std::vector<std::string> OrganicNetScene::getInteractionPointIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : interactionPoints_) ids.push_back(pair.first);
    return ids;
}

std::shared_ptr<Parameter> OrganicNetScene::getParameter(const std::string& paramId) {
    auto it = parameters_.find(paramId);
    return (it != parameters_.end()) ? it->second : nullptr;
}

bool OrganicNetScene::setParameter(const std::string& paramId, float value) {
    std::string key = paramId;
    if (key.find("organicNet.") == 0)
        key = key.substr(std::string("organicNet.").length());

    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        it->second->currentValue = value;

        if (key == "grid.rows") netParams_.rows = static_cast<int>(value);
        else if (key == "grid.cols") netParams_.cols = static_cast<int>(value);
        else if (key == "grid.spacing") netParams_.spacing = value;
        else if (key == "grid.randomOffset") {
            netParams_.randomOffsetX = value;
            netParams_.randomOffsetY = value;
        }
        else if (key == "grid.omitChance") netParams_.omitChance = value;
        else if (key == "phys.gravity") netParams_.gravity = value;
        else if (key == "phys.damping") netParams_.damping = value;
        else if (key == "phys.springStiffness") netParams_.springStiffness = value;
        else if (key == "phys.springIterations") netParams_.springIterations = static_cast<int>(value);
        else if (key == "noise.strength") netParams_.noiseStrength = value;
        else if (key == "noise.scale") netParams_.noiseScale = value;
        else if (key == "wind.strength") netParams_.windStrength = value;
        else if (key == "wind.frequency") netParams_.windFrequency = value;
        else if (key == "wind.bias") netParams_.windBias = value;
        else if (key == "lift.radius") netParams_.liftRadius = value;
        else if (key == "lift.strength") netParams_.liftStrength = value;
        else if (key == "return.force") netParams_.returnForce = value;
        else if (key == "line.segments") netParams_.lineSegments = static_cast<int>(value);
        else if (key == "line.noiseAmount") netParams_.lineNoiseAmount = value;
        else if (key == "line.alpha") netParams_.lineAlpha = value;
        else if (key == "line.width") netParams_.lineWidth = value;
        else if (key == "node.radius") netParams_.nodeRadius = value;
        else if (key == "node.alpha") netParams_.nodeAlpha = value;

        return true;
    }
    return false;
}

bool OrganicNetScene::setParameter(const std::string& paramId, const glm::vec2& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(value, 1.0f, ofGetWidth(), ofGetHeight());
    return true;
}

bool OrganicNetScene::setParameter(const std::string& paramId, const glm::vec3& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(value.x, value.y), 1.0f, ofGetWidth(), ofGetHeight(), value.z);
    return true;
}

std::vector<SuggestedMapping> OrganicNetScene::getSuggestedMappings() const { return {}; }

Preset OrganicNetScene::getDefaultPreset() const {
    Preset preset;
    preset.id = "default";
    preset.name = "Default";
    preset.description = "Valores por defecto de Organic Net";
    preset.sceneId = getId();
    preset.isDefault = true;
    for (const auto& pair : parameters_) {
        Parameter p = *pair.second;
        p.currentValue = p.defaultValue;
        preset.parameters.push_back(p);
    }
    return preset;
}

SceneMetadata OrganicNetScene::getSceneMetadata() const {
    SceneMetadata metadata;
    metadata.id = getId();
    metadata.name = getName();
    metadata.description = "Red organica flotante tipo red de pesca. Nodos conectados con lineas onduladas por noise. Los joints levantan la red desde abajo creando deformaciones suaves.";
    metadata.author = "Proyecto Danza";
    metadata.version = "1.0.0";
    metadata.tags = {"red", "organica", "malla", "flotante", "noise", "pesca"};
    metadata.category = "visual";
    metadata.hasInternalState = true;
    metadata.isTimeBased = false;
    metadata.estimatedDuration = 0.0f;
    metadata.presetPath = "";
    return metadata;
}

void OrganicNetScene::onBeat(int) {}
void OrganicNetScene::onBar(int) {}
bool OrganicNetScene::isValid() const { return isValid_; }
std::string OrganicNetScene::getInvalidReason() const { return invalidReason_; }
void OrganicNetScene::unloadResources() { interactionPoints_.clear(); }
void OrganicNetScene::reloadResources() { setup(); }
void OrganicNetScene::keyPressed(int) {}
void OrganicNetScene::keyReleased(int) {}
void OrganicNetScene::mousePressed(int, int, int) {}
void OrganicNetScene::mouseReleased(int, int, int) {}
void OrganicNetScene::mouseDragged(int, int, int) {}
void OrganicNetScene::mouseMoved(int, int) {}

void OrganicNetScene::windowResized(int, int) {
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) bg = param->currentValue;
    ofBackground(static_cast<int>(bg));
}

ScenePtr CreateOrganicNetScene() {
    return std::make_shared<OrganicNetScene>();
}

} // namespace modular
