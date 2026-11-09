#pragma once

// ============================================================================
// GenesisCommon — utilidades compartidas de las escenas volumétricas 3D.
//
// Paleta "Génesis Volumétrica" + mapeo 2D(píxel)+z → 3D(mundo) + cámara fija +
// geometría (convex hull / point-in-polygon) + "core motif" (núcleo palpitante).
//
// Header-only: funciones inline en namespace modular.
// ============================================================================

#include "ofMain.h"

#include <algorithm>
#include <vector>
#include <glm/vec2.hpp>

namespace modular {

// ============================================================================
// Paleta de color global
// ============================================================================

inline ofColor cNoche()   { return ofColor(29, 43, 83); }   // #1D2B53 Azul Noche
inline ofColor cMagenta() { return ofColor(126, 37, 83); }  // #7E2553 Púrpura Magenta
inline ofColor cNeon()    { return ofColor(255, 0, 77); }   // #FF004D Rosa/Rojo Neón
inline ofColor cCore()    { return ofColor(250, 239, 93); } // #FAEF5D Amarillo Eléctrico

// Azul tenue visible sobre el fondo cNoche (para geometría "en reposo").
inline ofColor cDim()     { return ofColor(58, 78, 140); }

// Color según proximidad (1=cerca → cCore, 0.5=medio → cNeon, 0=lejos → cMagenta).
inline ofColor proximityColor(float proximity) {
    proximity = ofClamp(proximity, 0.0f, 1.0f);
    if (proximity < 0.5f) {
        return cMagenta().getLerped(cNeon(), proximity * 2.0f);
    } else {
        return cNeon().getLerped(cCore(), (proximity - 0.5f) * 2.0f);
    }
}

// ============================================================================
// Mapeo de mundo: píxel 2D + profundidad normalizada → coordenadas 3D de mundo.
// z: 0 = cerca → z=+400, 1 = lejos → z=-800. Cámara fija en (0,0,1400).
// ============================================================================

constexpr float kWorldNearZ = 400.0f;
constexpr float kWorldFarZ  = -800.0f;
constexpr float kWorldHalfW = 800.0f;   // X mundo: -800..800
constexpr float kWorldHalfH = 600.0f;   // Y mundo: -600..600

inline ofVec3f toWorld(const glm::vec2& pixelPos, float z, float screenW, float screenH) {
    return ofVec3f(
        ofMap(pixelPos.x, 0.0f, screenW, -kWorldHalfW, kWorldHalfW),
        ofMap(pixelPos.y, 0.0f, screenH,  kWorldHalfH, -kWorldHalfH),
        ofMap(z, 0.0f, 1.0f, kWorldNearZ, kWorldFarZ)
    );
}

// Cámara 3D fija (NO orbitable; el ratón queda libre para interacción).
inline void beginGenesisCamera(ofCamera& cam, float w, float h) {
    cam.setPosition(0.0f, 0.0f, 1400.0f);
    cam.lookAt(ofVec3f(0.0f, 0.0f, 0.0f));
    cam.setFov(60.0f);
    cam.setAspectRatio(w / h);
    cam.begin();
}

// ============================================================================
// Geometría: convex hull (monotone chain) + point-in-polygon
// ============================================================================

inline std::vector<glm::vec2> computeConvexHull(std::vector<glm::vec2> points) {
    if (points.size() <= 2) return points;

    std::sort(points.begin(), points.end(), [](const glm::vec2& a, const glm::vec2& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });

    auto cross = [](const glm::vec2& o, const glm::vec2& a, const glm::vec2& b) -> float {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };

    std::vector<glm::vec2> hull;
    for (const auto& p : points) {
        while (hull.size() >= 2 && cross(hull[hull.size() - 2], hull.back(), p) <= 0.0f)
            hull.pop_back();
        hull.push_back(p);
    }
    size_t lowerSize = hull.size();
    for (int i = static_cast<int>(points.size()) - 2; i >= 0; i--) {
        while (hull.size() > lowerSize && cross(hull[hull.size() - 2], hull.back(), points[i]) <= 0.0f)
            hull.pop_back();
        hull.push_back(points[i]);
    }
    if (hull.size() > 1) hull.pop_back();  // remove duplicate of first
    return hull;
}

inline bool pointInPolygon(const glm::vec2& p, const std::vector<glm::vec2>& poly) {
    if (poly.size() < 3) return false;
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
            p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x) {
            inside = !inside;
        }
    }
    return inside;
}

// ============================================================================
// Core Motif — núcleo palpitante en el pecho (dibujado al final de cada draw).
// ============================================================================

inline void drawCoreMotif(const ofVec3f& chest, float now) {
    float r = 10.0f + 5.0f * sin(now * 3.0f);

    // Halo suave (aditivo) + núcleo sólido.
    ofPushStyle();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    ofSetColor(cCore(), 30);
    ofDrawSphere(chest, r * 3.5f);
    ofSetColor(cCore(), 70);
    ofDrawSphere(chest, r * 2.0f);
    ofDisableBlendMode();
    ofSetColor(cCore());
    ofDrawSphere(chest, r);
    ofPopStyle();
}

} // namespace modular
