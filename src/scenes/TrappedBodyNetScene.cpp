#include "scenes/TrappedBodyNetScene.h"
#include "registry/ParameterRegistry.h"
#include "adapters/kinect/KinectTypes.h"
#include <cmath>
#include <algorithm>

namespace modular {

// ============================================================================
// Constructor / Destructor
// ============================================================================

TrappedBodyNetScene::TrappedBodyNetScene()
    : currentPalette_(0)
    , isActive_(false)
    , isPaused_(false)
    , isValid_(true)
    , invalidReason_("")
    , parametersRegistered_(false)
    , debugMarkers_(false) {
}

TrappedBodyNetScene::~TrappedBodyNetScene() {
    unregisterParameters();
}

// ============================================================================
// Palette System
// ============================================================================

void TrappedBodyNetScene::setupPalettes() {
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

void TrappedBodyNetScene::buildNet() {
    nodes_.clear();
    edges_.clear();

    int R = netParams_.rings;
    int P = netParams_.pointsPerRing;
    float baseR = netParams_.baseRadius;
    float spacing = netParams_.ringSpacing;

    // Create nodes in rings
    nodes_.reserve(R * P);
    for (int ring = 0; ring < R; ring++) {
        for (int p = 0; p < P; p++) {
            RingNode node;
            float angle = (static_cast<float>(p) / static_cast<float>(P)) * TWO_PI;

            // Add noise to angle for organic distribution
            float angleNoise = ofSignedNoise(ring * 2.3f + p * 0.4f) * 0.12f;
            node.angle = angle + angleNoise;

            node.ringIndex = ring;
            node.baseRadius = baseR + ring * spacing;
            node.targetRadius = node.baseRadius;
            node.position = bodyCenter_
                + glm::vec2(std::cos(node.angle), std::sin(node.angle)) * node.baseRadius;
            node.velocity = glm::vec2(0.0f);
            node.noiseOffset = ofRandom(0.0f, 1000.0f);
            nodes_.push_back(node);
        }
    }

    auto idx = [P](int r, int p) -> int { return r * P + p; };

    // Same-ring connections (neighbors)
    for (int r = 0; r < R; r++) {
        for (int p = 0; p < P; p++) {
            int a = idx(r, p);
            int b = idx(r, (p + 1) % P);
            float dist = glm::distance(nodes_[a].position, nodes_[b].position);
            edges_.push_back({a, b, dist});
        }
    }

    // Cross-ring connections (closest angular match)
    for (int r = 0; r < R - 1; r++) {
        for (int p = 0; p < P; p++) {
            int a = idx(r, p);
            int b = idx(r + 1, p);
            float dist = glm::distance(nodes_[a].position, nodes_[b].position);
            edges_.push_back({a, b, dist});
        }
    }

    // Diagonal cross-ring connections (offset by 1-2 steps, ~60% random)
    for (int r = 0; r < R - 1; r++) {
        for (int p = 0; p < P; p++) {
            if (ofRandomuf() > 0.55f) continue;
            int offset = (ofRandomuf() < 0.5f) ? 1 : -1;
            int a = idx(r, p);
            int b = idx(r + 1, (p + offset + P) % P);
            float dist = glm::distance(nodes_[a].position, nodes_[b].position);
            edges_.push_back({a, b, dist});
        }
    }

    ofLogNotice("TrappedBodyNetScene") << "Built net: " << nodes_.size()
        << " nodes, " << edges_.size() << " edges";
}

void TrappedBodyNetScene::resetNet() {
    for (auto& node : nodes_) {
        node.targetRadius = node.baseRadius;
        node.position = bodyCenter_
            + glm::vec2(std::cos(node.angle), std::sin(node.angle)) * node.baseRadius;
        node.velocity = glm::vec2(0.0f);
    }
    openness_ = 0.0f;
}

// ============================================================================
// Body Center Computation
// ============================================================================

void TrappedBodyNetScene::computeBodyCenter() {
    glm::vec2 sum(0.0f);
    int count = 0;

    // Try canonical torso joints first
    const std::string keyJoints[] = {
        "shoulder_left", "shoulder_right", "hip_left", "hip_right"
    };
    for (const auto& name : keyJoints) {
        auto it = interactionPoints_.find(name);
        if (it != interactionPoints_.end() && it->second.active) {
            sum += it->second.position;
            count++;
        }
    }

    // Fallback: all active joints
    if (count == 0) {
        for (const auto& entry : interactionPoints_) {
            if (entry.second.active) {
                sum += entry.second.position;
                count++;
            }
        }
    }

    if (count > 0) {
        bodyCenter_ = sum / static_cast<float>(count);
    } else {
        // Default: center of screen
        bodyCenter_ = glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.45f);
    }
}

// ============================================================================
// Body Movement Computation
// ============================================================================

void TrappedBodyNetScene::computeBodyMovement(float deltaTime) {
    float totalMovement = 0.0f;
    int count = 0;

    for (const auto& entry : interactionPoints_) {
        if (entry.second.active) {
            float speed = glm::length(entry.second.velocity);
            totalMovement += speed;
            count++;
        }
    }

    float avgMovement = (count > 0) ? totalMovement / static_cast<float>(count) : 0.0f;

    // Smooth the movement signal
    float smoothing = 0.7f;
    prevMovement_ = prevMovement_ * smoothing + avgMovement * (1.0f - smoothing);

    updateOpenness(prevMovement_, deltaTime);
}

// ============================================================================
// Openness Dynamics
// ============================================================================

void TrappedBodyNetScene::updateOpenness(float movementAmount, float deltaTime) {
    if (movementAmount > 15.0f) {  // threshold: body is moving
        float openAmount = netParams_.openSpeed * (movementAmount / 200.0f) * deltaTime * 60.0f;
        openness_ = std::min(1.0f, openness_ + openAmount);
    } else {
        float closeAmount = netParams_.closeSpeed * deltaTime * 60.0f;
        openness_ = std::max(0.0f, openness_ - closeAmount);
    }
}

// ============================================================================
// Joint Opening Forces
// ============================================================================

void TrappedBodyNetScene::applyJointOpeningForces(float dt) {
    float dt60 = dt * 60.0f;

    for (const auto& ipEntry : interactionPoints_) {
        const InteractionPoint& ip = ipEntry.second;
        if (!ip.active) continue;

        for (auto& node : nodes_) {
            float dx = node.position.x - ip.position.x;
            float dy = node.position.y - ip.position.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < netParams_.interactionRadius && dist > 0.001f) {
                float t = 1.0f - dist / netParams_.interactionRadius;
                float strength = t * t * netParams_.openingStrength * dt60;

                // Direction: radially outward from body center
                glm::vec2 outward = node.position - bodyCenter_;
                float outLen = glm::length(outward);
                if (outLen > 0.001f) {
                    outward /= outLen;
                }

                node.velocity += outward * strength;
            }
        }
    }

    // Also push based on openness: expand target radii
    for (auto& node : nodes_) {
        int ring = node.ringIndex;
        int R = netParams_.rings;
        if (R < 1) R = 1;

        // ringInfluence: inner rings react more, outer rings catch up at higher openness
        float ringT = static_cast<float>(ring) / static_cast<float>(R);
        float ringInfluence = 1.0f - ringT * 0.4f;  // 1.0 → 0.6

        node.targetRadius = node.baseRadius
            + openness_ * netParams_.maxExpansion * ringInfluence;
    }
}

// ============================================================================
// Net Physics
// ============================================================================

void TrappedBodyNetScene::updateNetPhysics(float dt) {
    float dt60 = dt * 60.0f;
    float time = ofGetElapsedTimef();

    for (auto& node : nodes_) {
        // Elastic return: pull toward target radius
        glm::vec2 radial = node.position - bodyCenter_;
        float currentRadius = glm::length(radial);
        if (currentRadius > 0.001f) {
            glm::vec2 radialDir = radial / currentRadius;
            float radiusDiff = node.targetRadius - currentRadius;

            // Asymmetric: stronger pull when too close to center, gentler stretch
            float elasticForce;
            if (radiusDiff > 0) {
                // Need to expand — stronger push outward
                elasticForce = radiusDiff * netParams_.elasticReturn * 1.5f;
            } else {
                // Need to contract — gentler pull inward
                elasticForce = radiusDiff * netParams_.elasticReturn * 0.6f;
            }

            node.velocity += radialDir * elasticForce * dt60;
        }

        // Organic noise drift (tangential + radial)
        float nx = node.position.x * netParams_.noiseScale;
        float ny = node.position.y * netParams_.noiseScale;
        float driftTan = ofSignedNoise(nx + time * 0.5f, ny + node.noiseOffset * 0.01f)
                       * netParams_.noiseStrength * dt60 * 0.5f;
        float driftRad = ofSignedNoise(ny + time * 0.4f, nx - node.noiseOffset * 0.01f)
                       * netParams_.noiseStrength * dt60 * 0.3f;

        // Tangential direction (perpendicular to radial)
        if (currentRadius > 0.001f) {
            glm::vec2 tangent(-radial.y / currentRadius, radial.x / currentRadius);
            node.velocity += tangent * driftTan;
            node.velocity += (radial / currentRadius) * driftRad;
        }

        // Damping
        node.velocity *= netParams_.damping;

        // Integrate
        node.position += node.velocity * dt60;
    }
}

// ============================================================================
// Spring Constraints
// ============================================================================

void TrappedBodyNetScene::solveConnections() {
    float stiffness = netParams_.springStiffness;

    for (int iter = 0; iter < netParams_.springIterations; iter++) {
        for (const auto& edge : edges_) {
            RingNode& a = nodes_[edge.a];
            RingNode& b = nodes_[edge.b];

            glm::vec2 delta = b.position - a.position;
            float dist = glm::length(delta);
            if (dist < 0.001f) continue;

            float correction = (dist - edge.restLength) * stiffness;
            glm::vec2 dir = delta / dist;

            a.position += dir * correction * 0.5f;
            b.position -= dir * correction * 0.5f;

            a.velocity += dir * correction * 0.15f;
            b.velocity -= dir * correction * 0.15f;
        }
    }
}

// ============================================================================
// Organic Line Drawing
// ============================================================================

void TrappedBodyNetScene::drawOrganicLine(const glm::vec2& a, const glm::vec2& b,
                                           float noiseBase, float tension) {
    float time = ofGetElapsedTimef();
    int segs = netParams_.lineSegments;
    float nsAmount = netParams_.lineNoiseAmount;

    glm::vec2 dir = b - a;
    float len = glm::length(dir);
    if (len < 0.001f) return;

    glm::vec2 dirNorm = dir / len;
    glm::vec2 perp(-dirNorm.y, dirNorm.x);

    // Tension amplifies the noise for a more "alive" look
    float tensionFactor = 1.0f + tension * 0.6f;
    float amount = nsAmount * tensionFactor;

    ofBeginShape();
    for (int i = 0; i <= segs; i++) {
        float t = static_cast<float>(i) / static_cast<float>(segs);
        glm::vec2 base = a + dir * t;

        float nx = base.x * 0.018f;
        float ny = base.y * 0.018f;
        float n1 = ofSignedNoise(nx + time * 0.4f + noiseBase);
        float n2 = ofSignedNoise(ny + time * 0.6f + noiseBase * 1.2f) * 0.5f;
        float noiseVal = n1 + n2;

        // Taper near endpoints for clean connections
        float taper = 1.0f - std::abs(t * 2.0f - 1.0f);
        taper = taper * taper;
        float offset = noiseVal * amount * (0.35f + 0.65f * taper);

        glm::vec2 pt = base + perp * offset;
        ofVertex(pt.x, pt.y);
    }
    ofEndShape(false);
}

// ============================================================================
// Parameter Definitions
// ============================================================================

void TrappedBodyNetScene::initializeParameterDefinitions() {
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

    addParam("rings.count", "Anillos",
             3.0f, 10.0f, 6.0f, 1.0f,
             "Cantidad de anillos concentricos", ParameterType::INT);
    addParam("rings.points", "Puntos x Anillo",
             16.0f, 60.0f, 32.0f, 2.0f,
             "Puntos por anillo", ParameterType::INT);
    addParam("rings.baseRadius", "Radio Base",
             20.0f, 80.0f, 45.0f, 2.0f,
             "Radio del anillo mas interno (px)");
    addParam("rings.spacing", "Separacion Anillos",
             25.0f, 60.0f, 40.0f, 2.0f,
             "Separacion base entre anillos (px)");

    addParam("open.maxExpansion", "Expansion Max",
             100.0f, 400.0f, 250.0f, 10.0f,
             "Expansion maxima cuando openness=1.0 (px)");
    addParam("open.interactRadius", "Radio Interaccion",
             40.0f, 200.0f, 110.0f, 5.0f,
             "Radio de influencia de cada joint (px)");
    addParam("open.strength", "Fuerza Apertura",
             0.3f, 3.0f, 1.2f, 0.1f,
             "Fuerza radial de apertura por joint");

    addParam("elastic.return", "Retorno Elastico",
             0.002f, 0.04f, 0.01f, 0.001f,
             "Fuerza de retorno al radio objetivo");
    addParam("elastic.damping", "Amortiguacion",
             0.9f, 0.995f, 0.96f, 0.001f,
             "Friccion general");
    addParam("elastic.spring", "Rigidez Resortes",
             0.03f, 0.3f, 0.12f, 0.01f,
             "Fuerza de constraints entre nodos");
    addParam("elastic.springIter", "Iteraciones Resorte",
             1.0f, 6.0f, 3.0f, 1.0f,
             "Pasadas de relajacion por frame", ParameterType::INT);

    addParam("noise.strength", "Fuerza Noise",
             2.0f, 30.0f, 12.0f, 1.0f,
             "Intensidad del movimiento organico");
    addParam("noise.scale", "Escala Noise",
             0.005f, 0.04f, 0.018f, 0.001f,
             "Escala espacial del noise");

    addParam("dynamics.openSpeed", "Velocidad Apertura",
             0.005f, 0.08f, 0.025f, 0.005f,
             "Que tan rapido se abre con movimiento");
    addParam("dynamics.closeSpeed", "Velocidad Cierre",
             0.001f, 0.015f, 0.003f, 0.001f,
             "Que tan rapido se cierra sin movimiento");

    addParam("line.segments", "Segmentos Linea",
             4.0f, 20.0f, 12.0f, 1.0f,
             "Segmentos por linea organica", ParameterType::INT);
    addParam("line.noiseAmount", "Ondulacion Linea",
             3.0f, 25.0f, 8.0f, 1.0f,
             "Amplitud de ondulacion (px)");
    addParam("line.tensionAlphaBoost", "Brillo x Tension",
             0.0f, 2.0f, 0.5f, 0.1f,
             "Extra transparencia cuando la linea esta bajo tension");
    addParam("line.alpha", "Alpha Lineas",
             30.0f, 255.0f, 170.0f, 5.0f,
             "Transparencia base de lineas");
    addParam("line.width", "Grosor Linea",
             0.3f, 4.0f, 1.2f, 0.1f,
             "Grosor base de las lineas (px)");
    addParam("node.radius", "Radio Nodos",
             0.5f, 5.0f, 2.0f, 0.5f,
             "Radio de los nodos");
}

// ============================================================================
// Parameter Registration / Sync
// ============================================================================

void TrappedBodyNetScene::registerParameters() {
    if (parametersRegistered_) return;
    initializeParameterDefinitions();
    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (!param) continue;
        Parameter definition = *param;
        bool registered = registry.registerParam(definition);
        if (!registered && !registry.hasParam(definition.id)) {
            ofLogWarning("TrappedBodyNetScene")
                << "Failed to register parameter " << definition.id;
        }
    }
    parametersRegistered_ = true;
}

void TrappedBodyNetScene::unregisterParameters() {
    if (!parametersRegistered_) return;
    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (param) registry.unregisterParam(param->id);
    }
    parametersRegistered_ = false;
}

void TrappedBodyNetScene::syncParamsFromMap() {
    auto readFloat = [&](const std::string& key, float& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) target = it->second->currentValue;
    };
    auto readInt = [&](const std::string& key, int& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) target = static_cast<int>(it->second->currentValue);
    };

    readInt("rings.count", netParams_.rings);
    readInt("rings.points", netParams_.pointsPerRing);
    readFloat("rings.baseRadius", netParams_.baseRadius);
    readFloat("rings.spacing", netParams_.ringSpacing);
    readFloat("open.maxExpansion", netParams_.maxExpansion);
    readFloat("open.interactRadius", netParams_.interactionRadius);
    readFloat("open.strength", netParams_.openingStrength);
    readFloat("elastic.return", netParams_.elasticReturn);
    readFloat("elastic.damping", netParams_.damping);
    readFloat("elastic.spring", netParams_.springStiffness);
    readInt("elastic.springIter", netParams_.springIterations);
    readFloat("noise.strength", netParams_.noiseStrength);
    readFloat("noise.scale", netParams_.noiseScale);
    readFloat("dynamics.openSpeed", netParams_.openSpeed);
    readFloat("dynamics.closeSpeed", netParams_.closeSpeed);
    readInt("line.segments", netParams_.lineSegments);
    readFloat("line.noiseAmount", netParams_.lineNoiseAmount);
    readFloat("line.tensionAlphaBoost", netParams_.tensionAlphaBoost);
    readFloat("line.alpha", netParams_.lineAlpha);
    readFloat("line.width", netParams_.lineWidth);
    readFloat("node.radius", netParams_.nodeRadius);
}

// ============================================================================
// Lifecycle
// ============================================================================

void TrappedBodyNetScene::setup() {
    ofEnableAlphaBlending();
    setupPalettes();
    initializeParameterDefinitions();
    ofSetBackgroundAuto(true);
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) bg = param->currentValue;
    ofBackground(static_cast<int>(bg));
}

void TrappedBodyNetScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    interactionPoints_.clear();
    openness_ = 0.25f;
    prevMovement_ = 0.0f;

    ofSetWindowTitle("Trapped Body Net — Red que Atrapa el Cuerpo");
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

    bodyCenter_ = glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.45f);
    buildNet();
}

void TrappedBodyNetScene::deactivate() {
    isActive_ = false;
    unregisterParameters();
    interactionPoints_.clear();
    nodes_.clear();
    edges_.clear();
}

void TrappedBodyNetScene::pause() { isPaused_ = true; }
void TrappedBodyNetScene::resume() { isPaused_ = false; }

std::string TrappedBodyNetScene::getId() const { return "trappedBodyNet"; }
std::string TrappedBodyNetScene::getName() const { return "Trapped Body Net — Red Atrapada"; }

// ============================================================================
// Update
// ============================================================================

void TrappedBodyNetScene::update(float deltaTime) {
    if (!isActive_ || isPaused_ || nodes_.empty()) return;

    // 1. Update interaction points
    for (auto& entry : interactionPoints_) {
        entry.second.update(deltaTime);
    }

    // 2. Compute body center from torso joints
    computeBodyCenter();

    // 3. Compute body movement → update openness
    computeBodyMovement(deltaTime);

    // 4. Apply joint opening forces (radial push outward)
    applyJointOpeningForces(deltaTime);

    // 5. Physics: elastic return, noise drift, damping, integration
    updateNetPhysics(deltaTime);

    // 6. Spring constraints between connected nodes
    solveConnections();

    // 7. Update edge rest lengths to match openness (so net doesn't snap)
    for (auto& edge : edges_) {
        if (edge.a < 0 || edge.b < 0) continue;
        const RingNode& a = nodes_[edge.a];
        const RingNode& b = nodes_[edge.b];
        edge.restLength = glm::distance(a.position, b.position) * 0.3f
                        + edge.restLength * 0.7f;  // slow adaptation
    }
}

// ============================================================================
// Draw
// ============================================================================

void TrappedBodyNetScene::draw() {
    if (!isActive_ || nodes_.empty()) return;

    ofEnableAlphaBlending();
    ofNoFill();

    const auto& palette = palettes_[currentPalette_ % palettes_.size()];
    ofFloatColor lineColor = palette[0];
    float baseAlpha = netParams_.lineAlpha;

    // Depth relation: body proximity scales line width and node size.
    float depthScale = 0.7f + 0.6f * averageProximity(interactionPoints_);

    // Draw organic lines
    for (size_t e = 0; e < edges_.size(); e++) {
        const TrappedEdge& edge = edges_[e];
        if (edge.a < 0 || edge.b < 0) continue;

        const RingNode& a = nodes_[edge.a];
        const RingNode& b = nodes_[edge.b];

        float dist = glm::distance(a.position, b.position);
        float stretchRatio = (edge.restLength > 0.001f)
            ? dist / edge.restLength : 1.0f;
        float tension = glm::clamp(std::abs(stretchRatio - 1.0f), 0.0f, 1.0f);

        // More opaque when under tension
        float alphaMult = 1.0f + tension * netParams_.tensionAlphaBoost;
        int alphaByte = static_cast<int>(baseAlpha * alphaMult);
        alphaByte = std::min(255, alphaByte);

        ofSetColor(lineColor.r * 255.0f, lineColor.g * 255.0f, lineColor.b * 255.0f, alphaByte);
        ofSetLineWidth((netParams_.lineWidth + tension * 0.6f) * depthScale);

        drawOrganicLine(a.position, b.position, a.noiseOffset + e * 0.3f, tension);
    }

    // Draw subtle nodes
    ofFill();
    for (const auto& node : nodes_) {
        int aByte = static_cast<int>(netParams_.nodeRadius > 0.5f ? 80.0f : 40.0f);
        ofSetColor(lineColor.r * 255.0f, lineColor.g * 255.0f, lineColor.b * 255.0f, aByte);
        ofDrawCircle(node.position.x, node.position.y, netParams_.nodeRadius * depthScale);
    }

    // Body center marker (debug)
    if (debugMarkers_) {
        ofSetColor(255, 100, 100, 180);
        ofFill();
        ofDrawCircle(bodyCenter_.x, bodyCenter_.y, 8.0f);

        // Joint markers
        ofNoFill();
        ofSetColor(255, 0, 0, 120);
        for (const auto& entry : interactionPoints_) {
            if (entry.second.active) {
                ofDrawCircle(entry.second.position.x, entry.second.position.y, 5);
            }
        }
    }
}

// ============================================================================
// Parameter Access
// ============================================================================

std::vector<std::string> TrappedBodyNetScene::getParameterIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : parameters_) ids.push_back(pair.first);
    return ids;
}

std::vector<std::string> TrappedBodyNetScene::getInteractionPointIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : interactionPoints_) ids.push_back(pair.first);
    return ids;
}

std::shared_ptr<Parameter> TrappedBodyNetScene::getParameter(const std::string& paramId) {
    auto it = parameters_.find(paramId);
    return (it != parameters_.end()) ? it->second : nullptr;
}

bool TrappedBodyNetScene::setParameter(const std::string& paramId, float value) {
    std::string key = paramId;
    if (key.find("trappedBodyNet.") == 0)
        key = key.substr(std::string("trappedBodyNet.").length());

    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        it->second->currentValue = value;

        if (key == "rings.count") netParams_.rings = static_cast<int>(value);
        else if (key == "rings.points") netParams_.pointsPerRing = static_cast<int>(value);
        else if (key == "rings.baseRadius") netParams_.baseRadius = value;
        else if (key == "rings.spacing") netParams_.ringSpacing = value;
        else if (key == "open.maxExpansion") netParams_.maxExpansion = value;
        else if (key == "open.interactRadius") netParams_.interactionRadius = value;
        else if (key == "open.strength") netParams_.openingStrength = value;
        else if (key == "elastic.return") netParams_.elasticReturn = value;
        else if (key == "elastic.damping") netParams_.damping = value;
        else if (key == "elastic.spring") netParams_.springStiffness = value;
        else if (key == "elastic.springIter") netParams_.springIterations = static_cast<int>(value);
        else if (key == "noise.strength") netParams_.noiseStrength = value;
        else if (key == "noise.scale") netParams_.noiseScale = value;
        else if (key == "dynamics.openSpeed") netParams_.openSpeed = value;
        else if (key == "dynamics.closeSpeed") netParams_.closeSpeed = value;
        else if (key == "line.segments") netParams_.lineSegments = static_cast<int>(value);
        else if (key == "line.noiseAmount") netParams_.lineNoiseAmount = value;
        else if (key == "line.tensionAlphaBoost") netParams_.tensionAlphaBoost = value;
        else if (key == "line.alpha") netParams_.lineAlpha = value;
        else if (key == "line.width") netParams_.lineWidth = value;
        else if (key == "node.radius") netParams_.nodeRadius = value;

        return true;
    }
    return false;
}

bool TrappedBodyNetScene::setParameter(const std::string& paramId, const glm::vec2& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(value, 1.0f, ofGetWidth(), ofGetHeight());
    return true;
}

bool TrappedBodyNetScene::setParameter(const std::string& paramId, const glm::vec3& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(value.x, value.y), 1.0f, ofGetWidth(), ofGetHeight(), value.z);
    return true;
}

std::vector<SuggestedMapping> TrappedBodyNetScene::getSuggestedMappings() const { return {}; }

Preset TrappedBodyNetScene::getDefaultPreset() const {
    Preset preset;
    preset.id = "default";
    preset.name = "Default";
    preset.description = "Valores por defecto de Trapped Body Net";
    preset.sceneId = getId();
    preset.isDefault = true;
    for (const auto& pair : parameters_) {
        Parameter p = *pair.second;
        p.currentValue = p.defaultValue;
        preset.parameters.push_back(p);
    }
    return preset;
}

SceneMetadata TrappedBodyNetScene::getSceneMetadata() const {
    SceneMetadata metadata;
    metadata.id = getId();
    metadata.name = getName();
    metadata.description = "Red organica radial que envuelve el cuerpo. Los joints empujan la red desde adentro abriendola progresivamente. Al quedarse quieto el cuerpo, la red vuelve a cerrarse lentamente.";
    metadata.author = "Proyecto Danza";
    metadata.version = "1.0.0";
    metadata.tags = {"red", "atrapada", "radial", "membrana", "apertura", "cuerpo"};
    metadata.category = "visual";
    metadata.hasInternalState = true;
    metadata.isTimeBased = false;
    metadata.estimatedDuration = 0.0f;
    metadata.presetPath = "";
    return metadata;
}

void TrappedBodyNetScene::onBeat(int) {}
void TrappedBodyNetScene::onBar(int) {}
bool TrappedBodyNetScene::isValid() const { return isValid_; }
std::string TrappedBodyNetScene::getInvalidReason() const { return invalidReason_; }
void TrappedBodyNetScene::unloadResources() { interactionPoints_.clear(); }
void TrappedBodyNetScene::reloadResources() { setup(); }
void TrappedBodyNetScene::keyPressed(int) {}
void TrappedBodyNetScene::keyReleased(int) {}
void TrappedBodyNetScene::mousePressed(int, int, int) {}
void TrappedBodyNetScene::mouseReleased(int, int, int) {}
void TrappedBodyNetScene::mouseDragged(int, int, int) {}
void TrappedBodyNetScene::mouseMoved(int, int) {}

void TrappedBodyNetScene::windowResized(int, int) {
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) bg = param->currentValue;
    ofBackground(static_cast<int>(bg));
}

ScenePtr CreateTrappedBodyNetScene() {
    return std::make_shared<TrappedBodyNetScene>();
}

} // namespace modular
