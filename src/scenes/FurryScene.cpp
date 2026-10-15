#include "scenes/FurryScene.h"
#include "registry/ParameterRegistry.h"
#include "scenes/GenesisCommon.h"
#include <cmath>
#include <algorithm>

namespace modular {

// ============================================================================
// Constants
// ============================================================================

static constexpr int BASE_GRID_COLS = 105;
static constexpr int BASE_GRID_ROWS = 70;
static constexpr int SEGMENTS_PER_FIBER = 3;       // root → mid → tip
static constexpr int VERTS_PER_FIBER = 12;          // 2 segments × 6 vertices per quad (GL_TRIANGLES)

// ============================================================================
// Smoothstep helper (GLSL-style)
// ============================================================================

static float smoothstep(float edge0, float edge1, float x) {
    float t = std::max(0.0f, std::min(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

// ============================================================================
// Convex Hull / Point-in-Polygon — compartidos en scenes/GenesisCommon.h
// ============================================================================

static float distToPolygon(const glm::vec2& p, const std::vector<glm::vec2>& poly, glm::vec2& outNormal) {
    if (poly.size() < 2) { outNormal = glm::vec2(0.0f, -1.0f); return 0.0f; }
    float minDist = 1e30f;
    size_t n = poly.size();
    for (size_t i = 0; i < n; i++) {
        size_t j = (i + 1) % n;
        glm::vec2 edge = poly[j] - poly[i];
        float edgeLen = glm::length(edge);
        if (edgeLen < 0.001f) continue;
        glm::vec2 edgeDir = edge / edgeLen;
        glm::vec2 toPoint = p - poly[i];
        float t = glm::dot(toPoint, edgeDir);
        t = std::max(0.0f, std::min(edgeLen, t));
        glm::vec2 closest = poly[i] + edgeDir * t;
        float dist = glm::length(p - closest);
        if (dist < minDist) {
            minDist = dist;
            outNormal = glm::vec2(-edgeDir.y, edgeDir.x);
        }
    }
    return minDist;
}

// ============================================================================
// FurryScene Implementation
// ============================================================================

FurryScene::FurryScene()
    : currentPalette_(0)
    , gridCols_(0)
    , gridRows_(0)
    , contourOpacity_(0.6f)
    , timeAccum_(0.0f)
    , isActive_(false)
    , isPaused_(false)
    , isValid_(true)
    , invalidReason_("")
    , density_(0.7f)
    , fiberLength_(18.0f)
    , stiffness_(0.4f)
    , windStrength_(0.5f)
    , windSpeed_(0.8f)
    , mouseRadius_(150.0f)
    , mouseForce_(1.2f)
    , breathAmount_(0.3f)
    , paletteParam_(0.0f)
    , fiberThickness_(3.0f)
    , parametersRegistered_(false) {
}

FurryScene::~FurryScene() {
    unregisterParameters();
}

// ============================================================================
// Palettes — same 5 as PinturaFluorescenteScene
// ============================================================================

void FurryScene::setupPalettes() {
    palettes_.clear();

    // 0: Rosa / Magenta
    palettes_.push_back({
        ofFloatColor::fromHex(0xFF00FF),
        ofFloatColor::fromHex(0xFF1493),
        ofFloatColor::fromHex(0xFF0080),
        ofFloatColor::fromHex(0xFF69B4),
        ofFloatColor::fromHex(0xFF00CC)
    });
    // 1: Cian / Turquesa
    palettes_.push_back({
        ofFloatColor::fromHex(0x00FFFF),
        ofFloatColor::fromHex(0x00CED1),
        ofFloatColor::fromHex(0x40E0D0),
        ofFloatColor::fromHex(0x00BFFF),
        ofFloatColor::fromHex(0x04D9FF)
    });
    // 2: Verde
    palettes_.push_back({
        ofFloatColor::fromHex(0x00FF00),
        ofFloatColor::fromHex(0x39FF14),
        ofFloatColor::fromHex(0x32CD32),
        ofFloatColor::fromHex(0x00FA9A),
        ofFloatColor::fromHex(0x7CFC00)
    });
    // 3: Naranja
    palettes_.push_back({
        ofFloatColor::fromHex(0xFF6B00),
        ofFloatColor::fromHex(0xFF4500),
        ofFloatColor::fromHex(0xFF8C00),
        ofFloatColor::fromHex(0xFF7F50),
        ofFloatColor::fromHex(0xFF6347)
    });
    // 4: Amarillo
    palettes_.push_back({
        ofFloatColor::fromHex(0xFFFF00),
        ofFloatColor::fromHex(0xFFD700),
        ofFloatColor::fromHex(0xCCFF00),
        ofFloatColor::fromHex(0xFFFF33),
        ofFloatColor::fromHex(0xE5FF00)
    });
}

// ============================================================================
// Grid rebuild — called on setup and windowResized
// ============================================================================

void FurryScene::rebuildGrid() {
    int cols = static_cast<int>(BASE_GRID_COLS * density_);
    int rows = static_cast<int>(BASE_GRID_ROWS * density_);
    cols = std::max(cols, 10);
    rows = std::max(rows, 10);

    gridCols_ = cols;
    gridRows_ = rows;

    float w = static_cast<float>(ofGetWidth());
    float h = static_cast<float>(ofGetHeight());

    float spacingX = w / static_cast<float>(cols);
    float spacingY = h / static_cast<float>(rows);

    size_t totalFibers = static_cast<size_t>(cols) * static_cast<size_t>(rows);

    rootPoints_.resize(totalFibers);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            size_t idx = static_cast<size_t>(r) * cols + c;
            rootPoints_[idx] = glm::vec2(
                spacingX * 0.5f + c * spacingX,
                spacingY * 0.5f + r * spacingY
            );
        }
    }

    // Pre-allocate VBO arrays: each fiber = 2 line segments = 4 vertices
    size_t totalVerts = totalFibers * VERTS_PER_FIBER;
    vertices_.resize(totalVerts, glm::vec3(0.0f));
    colors_.resize(totalVerts, ofFloatColor(0.0f, 0.0f, 0.0f, 0.0f));
}

// ============================================================================
// Parameter Definitions & Registry
// ============================================================================

void FurryScene::initializeParameterDefinitions() {
    if (!parameters_.empty()) {
        return;
    }

    const std::string sceneId = getId();
    auto addParam = [&](const std::string& key,
                        const std::string& label,
                        float minVal,
                        float maxVal,
                        float defaultVal,
                        float stepVal,
                        const std::string& description) {
        auto param = std::make_shared<Parameter>();
        param->id = sceneId + "." + key;
        param->label = label;
        param->type = ParameterType::FLOAT;
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

    addParam("density", "densidad", 0.3f, 1.0f, 0.7f, 0.05f,
             "Multiplicador de densidad de la grilla de fibras");
    addParam("fiberLength", "largoFibra", 5.0f, 40.0f, 18.0f, 1.0f,
             "Largo base de cada fibra en pixeles");
    addParam("stiffness", "rigidez", 0.0f, 1.0f, 0.4f, 0.05f,
             "Resistencia al doblado (mas alto = mas rigido)");
    addParam("windStrength", "viento", 0.0f, 2.0f, 0.5f, 0.05f,
             "Intensidad del viento basado en ruido Perlin");
    addParam("windSpeed", "velViento", 0.1f, 3.0f, 0.8f, 0.1f,
             "Velocidad de la animacion del viento");
    addParam("mouseRadius", "radioMouse", 50.0f, 400.0f, 150.0f, 10.0f,
             "Radio de influencia del mouse/interaccion");
    addParam("mouseForce", "fuerzaMouse", 0.0f, 3.0f, 1.2f, 0.1f,
             "Fuerza de deflexion del mouse");
    addParam("breathAmount", "respiracion", 0.0f, 1.0f, 0.3f, 0.05f,
             "Amplitud de la oscilacion de respiracion");
    addParam("fiberThickness", "grosorFibra", 1.0f, 8.0f, 3.0f, 0.5f,
             "Grosor de cada fibra en pixeles");
    addParam("palette", "paleta", 0.0f, 4.0f, 0.0f, 1.0f,
             "Indice de paleta de colores (0-4)");
}

void FurryScene::registerParameters() {
    if (parametersRegistered_) {
        return;
    }

    initializeParameterDefinitions();
    auto& registry = ParameterRegistry::getGlobal();

    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (!param) continue;

        Parameter definition = *param;
        bool registered = registry.registerParam(definition);
        if (!registered && !registry.hasParam(definition.id)) {
            ofLogWarning("FurryScene")
                << "Failed to register parameter " << definition.id;
        }
    }

    parametersRegistered_ = true;
}

void FurryScene::unregisterParameters() {
    if (!parametersRegistered_) {
        return;
    }

    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (param) {
            registry.unregisterParam(param->id);
        }
    }

    parametersRegistered_ = false;
}

// ============================================================================
// Lifecycle
// ============================================================================

void FurryScene::setup() {
    setupPalettes();
    initializeParameterDefinitions();
    rebuildGrid();
    registerParameters();  // Register now so load can find them
}

void FurryScene::update(float deltaTime) {
    if (!isActive_ || isPaused_) {
        return;
    }

    // Accumulate time for noise animation (pause-aware)
    timeAccum_ += deltaTime;

    // Update interaction points: apply decay for points no longer receiving data
    for (auto& entry : interactionPoints_) {
        entry.second.update(deltaTime);
    }

    // Build body convex hull from active interaction points.
    // The hull acts as a solid obstacle that pushes fibers outward.
    bodyHull_.clear();
    {
        std::vector<glm::vec2> activePoints;
        for (const auto& entry : interactionPoints_) {
            if (entry.second.active) {
                activePoints.push_back(entry.second.position);
            }
        }
        if (activePoints.size() >= 3) {
            bodyHull_ = computeConvexHull(activePoints);
            // Shrink hull 5% toward centroid so contour sits slightly inside the body
            if (bodyHull_.size() >= 3) {
                glm::vec2 centroid(0.0f);
                for (const auto& v : bodyHull_) centroid += v;
                centroid /= static_cast<float>(bodyHull_.size());
                for (auto& v : bodyHull_) {
                    v = centroid + (v - centroid) * 0.91f;
                }
            }
        }
    }

    // Sync palette from GLOBAL parameter (ignore local palette param)
    auto& registry = ParameterRegistry::getGlobal();
    if (auto globalPalette = registry.getParam("global.currentPalette")) {
        int newPalette = static_cast<int>(globalPalette->currentValue);
        if (newPalette != currentPalette_) {
            currentPalette_ = newPalette;
            paletteParam_ = static_cast<float>(newPalette);
        }
    }
    currentPalette_ = std::max(0, std::min(currentPalette_, static_cast<int>(palettes_.size()) - 1));

    const auto& palette = palettes_[currentPalette_];
    const size_t paletteSize = palette.size();

    const float t = timeAccum_;
    const size_t totalFibers = rootPoints_.size();

    for (size_t i = 0; i < totalFibers; i++) {
        const glm::vec2& root = rootPoints_[i];

        // Spatial noise coordinates (scaled down for smooth variation)
        float nx = root.x * 0.005f;
        float ny = root.y * 0.005f;

        // ----- Base inclination angle from Perlin noise -----
        float baseAngle = ofNoise(nx, ny, t * windSpeed_ * 0.3f) * glm::two_pi<float>();

        // ----- Wind deflection -----
        float windAngle = ofNoise(nx * 2.0f, ny * 2.0f, t * windSpeed_) * glm::two_pi<float>();
        float windMag = (ofNoise(nx * 1.5f, ny * 1.5f, t * windSpeed_ * 0.7f) - 0.5f) * 2.0f * windStrength_;

        // ----- Breathing oscillation -----
        float breath = std::sin(t * 1.5f + nx * 10.0f + ny * 10.0f) * breathAmount_;

        // ----- Length variation per fiber from noise -----
        float lengthNoise = ofNoise(nx * 3.0f, ny * 3.0f, t * 0.1f);
        float segLen = fiberLength_ * (0.6f + 0.4f * lengthNoise + breath * 0.3f) / static_cast<float>(SEGMENTS_PER_FIBER);

        // ----- Mouse/interaction perturbation -----
        // Find nearest active interaction point and compute deflection
        glm::vec2 deflection(0.0f, 0.0f);
        {
            float nearestDist = mouseRadius_ + 1.0f;
            glm::vec2 nearestPos(0.0f);
            bool hasActive = false;

            for (const auto& entry : interactionPoints_) {
                const auto& pt = entry.second;
                if (!pt.active) continue;

                float d = glm::length(root - pt.position);
                if (d < nearestDist) {
                    nearestDist = d;
                    nearestPos = pt.position;
                    hasActive = true;
                }
            }

            if (hasActive && nearestDist < mouseRadius_ && mouseRadius_ > 0.0f) {
                float influence = 1.0f - smoothstep(0.0f, mouseRadius_, nearestDist);
                glm::vec2 awayDir = (nearestDist > 0.001f)
                    ? glm::normalize(root - nearestPos)
                    : glm::vec2(0.0f, -1.0f);
                deflection = awayDir * influence * mouseForce_ * fiberLength_ * 0.5f;
            }
        }

        // ----- Body silhouette deflection -----
        // Fibers whose root is inside the body hull are removed entirely.
        // Fibers near the outside edge are pushed away from the body.
        bool fiberInsideBody = false;
        if (!bodyHull_.empty() && contourOpacity_ > 0.0f) {
            if (pointInPolygon(root, bodyHull_)) {
                fiberInsideBody = true;
            } else {
                // Find closest edge and distance
                float minDist = 1e30f;
                glm::vec2 closestPt(0.0f);
                size_t hullN = bodyHull_.size();
                for (size_t j = 0; j < hullN; j++) {
                    size_t k = (j + 1) % hullN;
                    glm::vec2 edge = bodyHull_[k] - bodyHull_[j];
                    float edgeLen = glm::length(edge);
                    if (edgeLen < 0.001f) continue;
                    glm::vec2 edgeDir = edge / edgeLen;
                    glm::vec2 toPt = root - bodyHull_[j];
                    float t = glm::dot(toPt, edgeDir);
                    t = std::max(0.0f, std::min(edgeLen, t));
                    glm::vec2 cp = bodyHull_[j] + edgeDir * t;
                    float d = glm::length(root - cp);
                    if (d < minDist) { minDist = d; closestPt = cp; }
                }
                const float influenceZone = 88.0f;  // 80 + 10%
                if (minDist < influenceZone) {
                    float influence = 1.0f - smoothstep(0.0f, influenceZone, minDist);
                    glm::vec2 awayDir = (minDist > 0.001f)
                        ? glm::normalize(root - closestPt)
                        : glm::vec2(0.0f, -1.0f);
                    deflection += awayDir * influence * mouseForce_ * fiberLength_ * 1.65f;  // +10%
                }
            }
        }

        // ----- Build the 3 points: root, mid, tip -----
        // Combine base angle + wind + stiffness
        float combinedAngle = baseAngle + windAngle * windMag;

        // Stiffness makes fibers tend toward "straight up" (angle = -PI/2)
        float stiffAngle = glm::mix(combinedAngle, -glm::half_pi<float>(), stiffness_);

        glm::vec2 dir1(std::cos(stiffAngle), std::sin(stiffAngle));
        glm::vec2 mid = root + dir1 * segLen + deflection * 0.5f;

        // Tip: slightly more curved (second noise octave)
        float tipAngle = stiffAngle + (ofNoise(nx * 4.0f, ny * 4.0f, t * windSpeed_ * 1.2f) - 0.5f) * (1.0f - stiffness_) * 2.0f;
        glm::vec2 dir2(std::cos(tipAngle), std::sin(tipAngle));
        glm::vec2 tip = mid + dir2 * segLen + deflection;

        // ----- Color by grid zone + gradient along fiber -----
        // Zone-based color selection: use grid position
        int row = static_cast<int>(i) / gridCols_;
        int col = static_cast<int>(i) % gridCols_;
        float zoneVal = ofNoise(col * 0.08f, row * 0.08f);
        int colorIdx = static_cast<int>(zoneVal * paletteSize) % static_cast<int>(paletteSize);
        ofFloatColor baseColor = palette[colorIdx];

        // Root: darker, less alpha.  Tip: brighter, full alpha.
        float rootAlpha = 0.2f;
        float midAlpha = 0.6f;
        float tipAlpha = 1.0f;

        float rootBright = 0.3f;
        float tipBright = 1.0f;

        ofFloatColor rootColor(baseColor.r * rootBright, baseColor.g * rootBright, baseColor.b * rootBright, rootAlpha);
        ofFloatColor midColor(baseColor.r * 0.7f, baseColor.g * 0.7f, baseColor.b * 0.7f, midAlpha);
        ofFloatColor tipColor(baseColor.r * tipBright, baseColor.g * tipBright, baseColor.b * tipBright, tipAlpha);

        // ----- Write to VBO as quads (GL_TRIANGLES) with fiber thickness -----
        // Thickness varies per fiber using spatial noise (+30% base, with variation)
        float thickNoise = ofNoise(nx * 0.7f, ny * 0.7f, t * 0.03f);
        float variedThick = fiberThickness_ * 1.56f * (0.3f + 1.4f * std::sqrt(thickNoise));
        float halfThick = variedThick * 0.5f;

        size_t vi = i * VERTS_PER_FIBER;

        // Fibers whose root is inside the body hull are invisible
        if (fiberInsideBody) {
            ofFloatColor transparent(0.0f, 0.0f, 0.0f, 0.0f);
            for (int j = 0; j < VERTS_PER_FIBER; j++) {
                vertices_[vi + j] = glm::vec3(0.0f);
                colors_[vi + j] = transparent;
            }
        } else {
        // Segment 1 perpendicular (root → mid)
        glm::vec2 seg1Dir = mid - root;
        float seg1Len = glm::length(seg1Dir);
        glm::vec2 seg1Perp(0.0f, halfThick);
        if (seg1Len > 0.001f) {
            seg1Dir /= seg1Len;
            seg1Perp = glm::vec2(-seg1Dir.y, seg1Dir.x) * halfThick;
        }

        size_t vi = i * VERTS_PER_FIBER;

        // Triangle 1: root+perp, root-perp, mid-perp
        vertices_[vi]     = glm::vec3(root.x + seg1Perp.x, root.y + seg1Perp.y, 0.0f);
        vertices_[vi + 1] = glm::vec3(root.x - seg1Perp.x, root.y - seg1Perp.y, 0.0f);
        vertices_[vi + 2] = glm::vec3(mid.x - seg1Perp.x, mid.y - seg1Perp.y, 0.0f);
        colors_[vi]       = rootColor;
        colors_[vi + 1]   = rootColor;
        colors_[vi + 2]   = midColor;

        // Triangle 2: root+perp, mid-perp, mid+perp
        vertices_[vi + 3] = glm::vec3(root.x + seg1Perp.x, root.y + seg1Perp.y, 0.0f);
        vertices_[vi + 4] = glm::vec3(mid.x - seg1Perp.x, mid.y - seg1Perp.y, 0.0f);
        vertices_[vi + 5] = glm::vec3(mid.x + seg1Perp.x, mid.y + seg1Perp.y, 0.0f);
        colors_[vi + 3]   = rootColor;
        colors_[vi + 4]   = midColor;
        colors_[vi + 5]   = midColor;

        // Segment 2 perpendicular (mid → tip)
        glm::vec2 seg2Dir = tip - mid;
        float seg2Len = glm::length(seg2Dir);
        glm::vec2 seg2Perp(0.0f, halfThick);
        if (seg2Len > 0.001f) {
            seg2Dir /= seg2Len;
            seg2Perp = glm::vec2(-seg2Dir.y, seg2Dir.x) * halfThick;
        }

        // Triangle 3: mid+perp, mid-perp, tip-perp
        vertices_[vi + 6]  = glm::vec3(mid.x + seg2Perp.x, mid.y + seg2Perp.y, 0.0f);
        vertices_[vi + 7]  = glm::vec3(mid.x - seg2Perp.x, mid.y - seg2Perp.y, 0.0f);
        vertices_[vi + 8]  = glm::vec3(tip.x - seg2Perp.x, tip.y - seg2Perp.y, 0.0f);
        colors_[vi + 6]    = midColor;
        colors_[vi + 7]    = midColor;
        colors_[vi + 8]    = tipColor;

        // Triangle 4: mid+perp, tip-perp, tip+perp
        vertices_[vi + 9]  = glm::vec3(mid.x + seg2Perp.x, mid.y + seg2Perp.y, 0.0f);
        vertices_[vi + 10] = glm::vec3(tip.x - seg2Perp.x, tip.y - seg2Perp.y, 0.0f);
        vertices_[vi + 11] = glm::vec3(tip.x + seg2Perp.x, tip.y + seg2Perp.y, 0.0f);
        colors_[vi + 9]    = midColor;
        colors_[vi + 10]   = tipColor;
        colors_[vi + 11]   = tipColor;
        } // end else (fiber visible)
    }

    // Upload to VBO
    if (!vertices_.empty()) {
        vbo_.setVertexData(vertices_.data(), static_cast<int>(vertices_.size()), GL_DYNAMIC_DRAW);
        vbo_.setColorData(colors_.data(), static_cast<int>(colors_.size()), GL_DYNAMIC_DRAW);
    }
}

void FurryScene::draw() {
    if (!isActive_) {
        return;
    }

    ofEnableAlphaBlending();

    if (!vertices_.empty()) {
        vbo_.draw(GL_TRIANGLES, 0, static_cast<int>(vertices_.size()));
    }

    // Body contour from active mapped joints
    drawContour();
}

// ============================================================================
// Body Contour — draws body presence + pushes fibers
// ============================================================================

void FurryScene::drawContour() {
    if (contourOpacity_ <= 0.0f || bodyHull_.size() < 3) return;

    // Count active joints for opacity scaling
    int activeCount = 0;
    for (const auto& entry : interactionPoints_) {
        if (entry.second.active) activeCount++;
    }
    if (activeCount == 0) return;

    float alpha = contourOpacity_ * std::min(1.0f, activeCount / 12.0f);

    // Depth relation: closer body → more visible contour.
    alpha *= 0.6f + 0.8f * averageProximity(interactionPoints_);

    ofEnableAlphaBlending();

    // Draw filled body silhouette (dark translucent fill)
    ofSetColor(40, 30, 20, static_cast<int>(alpha * 80));
    ofFill();
    ofBeginShape();
    for (const auto& v : bodyHull_) {
        ofVertex(v.x, v.y);
    }
    ofEndShape(true);
}

void FurryScene::activate() {
    isActive_ = true;
    isPaused_ = false;

    ofSetWindowTitle("Fibras Vivas");
    ofSetFrameRate(60);
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) {
        bg = param->currentValue;
    }
    ofBackground(static_cast<int>(bg));
    
    // Leer paleta global
    if (auto paletteParam = registry.getParam("global.currentPalette")) {
        currentPalette_ = static_cast<int>(paletteParam->currentValue);
        paletteParam_ = static_cast<float>(currentPalette_);
    }

    registerParameters();
}

void FurryScene::deactivate() {
    isActive_ = false;

    unregisterParameters();
}

void FurryScene::pause() {
    isPaused_ = true;
}

void FurryScene::resume() {
    isPaused_ = false;
}

// ============================================================================
// Identification
// ============================================================================

std::string FurryScene::getId() const {
    return "furry";
}

std::string FurryScene::getName() const {
    return "Fibras Vivas";
}

// ============================================================================
// Parameter Management
// ============================================================================

std::vector<std::string> FurryScene::getParameterIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : parameters_) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::vector<std::string> FurryScene::getInteractionPointIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : interactionPoints_) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::shared_ptr<Parameter> FurryScene::getParameter(const std::string& paramId) {
    auto it = parameters_.find(paramId);
    if (it != parameters_.end()) {
        return it->second;
    }
    return nullptr;
}

bool FurryScene::setParameter(const std::string& paramId, float value) {
    // Handle both "density" and "furry.density" formats
    std::string key = paramId;
    if (key.find("furry.") == 0) {
        key = key.substr(std::string("furry.").length());
    }
    
    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        // Update local member - use key (clean ID without prefix)
        if (key == "density") {
            float oldDensity = density_;
            density_ = value;
            // Rebuild grid if density changed significantly
            if (std::abs(density_ - oldDensity) > 0.01f) {
                rebuildGrid();
            }
        }
        else if (key == "fiberLength") fiberLength_ = value;
        else if (key == "stiffness") stiffness_ = value;
        else if (key == "windStrength") windStrength_ = value;
        else if (key == "windSpeed") windSpeed_ = value;
        else if (key == "mouseRadius") mouseRadius_ = value;
        else if (key == "mouseForce") mouseForce_ = value;
        else if (key == "breathAmount") breathAmount_ = value;
        else if (key == "fiberThickness") fiberThickness_ = value;
        else if (key == "palette") {
            paletteParam_ = value;
            currentPalette_ = static_cast<int>(std::floor(value));
            currentPalette_ = std::max(0, std::min(currentPalette_, static_cast<int>(palettes_.size()) - 1));
        }

        // Update stored parameter value
        it->second->currentValue = value;
        return true;
    }
    return false;
}

bool FurryScene::setParameter(const std::string& paramId, const glm::vec2& value) {
    // Values arrive normalized (0-1) from Kinect or pixel from mouse.
    // InteractionPoint::setTarget handles normalized→pixel conversion.
    // Mouse coordinates are also passed as normalized (from SceneEngine::mouseMoved).
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.setTarget(value, 1.0f, ofGetWidth(), ofGetHeight());
    return true;
}

bool FurryScene::setParameter(const std::string& paramId, const glm::vec3& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(value.x, value.y), 1.0f, ofGetWidth(), ofGetHeight(), value.z);
    return true;
}

// ============================================================================
// Mappings, Presets & Metadata
// ============================================================================

std::vector<SuggestedMapping> FurryScene::getSuggestedMappings() const {
    std::vector<SuggestedMapping> mappings;

    SuggestedMapping m;

    m = {};
    m.parameterId = "windStrength";
    m.sourceType = "osc";
    m.channel = "/furry/wind";
    m.control = "float";
    m.minInput = 0.0f;
    m.maxInput = 1.0f;
    m.description = "Intensidad del viento";
    mappings.push_back(m);

    m = {};
    m.parameterId = "breathAmount";
    m.sourceType = "osc";
    m.channel = "/furry/breath";
    m.control = "float";
    m.minInput = 0.0f;
    m.maxInput = 1.0f;
    m.description = "Amplitud de respiracion";
    mappings.push_back(m);

    return mappings;
}

Preset FurryScene::getDefaultPreset() const {
    Preset preset;
    preset.id = "default";
    preset.name = "Default";
    preset.description = "Valores por defecto de Fibras Vivas";
    preset.sceneId = getId();
    preset.isDefault = true;

    for (const auto& pair : parameters_) {
        Parameter p = *pair.second;
        p.currentValue = p.defaultValue;
        preset.parameters.push_back(p);
    }

    return preset;
}

SceneMetadata FurryScene::getSceneMetadata() const {
    SceneMetadata metadata;
    metadata.id = getId();
    metadata.name = getName();
    metadata.description = "Campo de fibras vivas con efecto de pelo dinamico, viento Perlin y perturbacion interactiva";
    metadata.author = "Proyecto Danza";
    metadata.version = "1.0.0";
    metadata.tags = {"fibras", "pelo", "perlin", "interactivo"};
    metadata.category = "visual";
    metadata.hasInternalState = true;
    metadata.isTimeBased = true;
    metadata.estimatedDuration = 0.0f;
    metadata.presetPath = "";
    return metadata;
}

// ============================================================================
// Timing & Sync
// ============================================================================

void FurryScene::onBeat(int beatNumber) {
    (void)beatNumber;
}

void FurryScene::onBar(int barNumber) {
    (void)barNumber;
}

// ============================================================================
// Validation
// ============================================================================

bool FurryScene::isValid() const {
    return isValid_;
}

std::string FurryScene::getInvalidReason() const {
    return invalidReason_;
}

// ============================================================================
// Resource Management
// ============================================================================

void FurryScene::unloadResources() {
    vertices_.clear();
    colors_.clear();
    rootPoints_.clear();
    vbo_.clear();
}

void FurryScene::reloadResources() {
    setup();
}

// ============================================================================
// Input Handlers
// ============================================================================

void FurryScene::keyPressed(int key) {
    if (key == 'p' || key == 'P') {
        currentPalette_ = (currentPalette_ + 1) % static_cast<int>(palettes_.size());
        paletteParam_ = static_cast<float>(currentPalette_);
        // Sync to parameter storage
        auto it = parameters_.find("palette");
        if (it != parameters_.end()) {
            it->second->currentValue = paletteParam_;
        }
    }
}

void FurryScene::keyReleased(int key) {
    (void)key;
}

void FurryScene::mousePressed(int x, int y, int button) {
    // No interact on click - only movement/dragging
    (void)x;
    (void)y;
    (void)button;
}

void FurryScene::mouseReleased(int x, int y, int button) {
    (void)x;
    (void)y;
    (void)button;
    // Interaction point now managed via setParameter("mouse", vec2) from SceneEngine
}

void FurryScene::mouseDragged(int x, int y, int button) {
    (void)button;
    // Interaction point now managed via setParameter("mouse", vec2) from SceneEngine
    (void)x;
    (void)y;
}

void FurryScene::mouseMoved(int x, int y) {
    // Interaction point now managed via setParameter("mouse", vec2) from SceneEngine
    (void)x;
    (void)y;
}

void FurryScene::windowResized(int w, int h) {
    (void)w;
    (void)h;
    rebuildGrid();
}

// ============================================================================
// Factory Function
// ============================================================================

ScenePtr CreateFurryScene() {
    return std::make_shared<FurryScene>();
}

} // namespace modular
