#pragma once

#include <glm/vec2.hpp>
#include <string>
#include "ofMain.h"

namespace modular {

// ============================================================================
// InteractionPoint — unidad de interacción con decay y trayectoria
// ============================================================================
// Cada escena almacena un map<string, InteractionPoint> para soportar
// múltiples fuentes de interacción (Kinect joints + mouse sintético).
// El punto convierte coordenadas normalizadas → pixeles y aplica decay
// lineal cuando la fuente deja de emitir datos.

struct InteractionPoint {
    glm::vec2 position = glm::vec2(0.0f);      // posición actual en PIXELES
    glm::vec2 velocity = glm::vec2(0.0f);      // px/s para inercia durante decay
    float confidence = 0.0f;                    // 0.0-1.0, baja con decay cuando no hay datos
    float z = 0.5f;                             // profundidad normalizada: 0=near, 1=far (Orbbec)
    float decayRate = DEFAULT_DECAY_RATE;       // configurable, ~3.3s fade con 0.3
    bool active = false;                        // contribuye a efectos visuales
    bool touchedThisFrame = false;              // true si setTarget fue llamado este frame
    std::string sourceName;                     // "hand_left", "mouse", etc.
    float lastUpdateTime = 0.0f;                // para cálculo de velocidad

    static constexpr float DEFAULT_DECAY_RATE = 0.3f;
    static constexpr int MAX_POINTS_PER_SCENE = 8;

    // Convierte coordenadas normalizadas (0-1) → pixeles y almacena.
    // Calcula velocidad en px/s para usar como inercia durante frames sin datos.
    // Cuando se recibe un dato nuevo, la posición es autoritativa (no se aplica inercia).
    // z (opcional) es la profundidad normalizada 0..1 (0=cerca, 1=lejos).
    void setTarget(glm::vec2 normalizedPos, float conf,
                   float screenW, float screenH, float depthZ = 0.5f) {
        glm::vec2 newPos(normalizedPos.x * screenW, normalizedPos.y * screenH);

        float now = ofGetElapsedTimef();
        float dt = now - lastUpdateTime;
        if (dt > 0.0001f && active) {
            velocity = (newPos - position) / dt;
        }

        position = newPos;
        confidence = conf;
        z = depthZ;
        active = true;
        touchedThisFrame = true;  // mark: data received this frame
        lastUpdateTime = now;
    }

    // Aplica decay lineal: confidence -= decayRate * deltaTime.
    // Solo aplica inercia (velocity) si NO se recibieron datos este frame.
    // Cuando confidence <= 0, desactiva el punto.
    void update(float deltaTime) {
        if (!active) return;

        confidence -= decayRate * deltaTime;

        // Only apply inertia when no fresh data arrived this frame.
        // When setTarget was called, position is already authoritative.
        if (!touchedThisFrame) {
            position += velocity * deltaTime;
        }
        touchedThisFrame = false;

        if (confidence <= 0.0f) {
            confidence = 0.0f;
            active = false;
        }
    }

    // Posición 3D de mundo (para escenas volumétricas). Mapea el píxel 2D a un
    // plano de mundo fijo y la profundidad normalizada a Z (0=cerca→+400, 1=lejos→-800).
    ofVec3f worldPos(float screenW, float screenH) const {
        return ofVec3f(
            ofMap(position.x, 0.0f, screenW, -800.0f, 800.0f),
            ofMap(position.y, 0.0f, screenH,  600.0f, -600.0f),
            ofMap(z, 0.0f, 1.0f, 400.0f, -800.0f)
        );
    }
};

// ============================================================================
// averageProximity — profundidad promedio del cuerpo (0=lejos, 1=cerca).
// 1.0 - z  convierte la profundidad normalizada (0=cerca, 1=lejos) a "proximidad".
// Funciona con cualquier contenedor de pares <key, InteractionPoint>.
// ============================================================================

template <typename Container>
inline float averageProximity(const Container& points, float fallback = 0.5f) {
    float sum = 0.0f;
    int count = 0;
    for (const auto& entry : points) {
        if (entry.second.active) {
            sum += 1.0f - entry.second.z;  // z=0 → proximity 1 (cerca)
            count++;
        }
    }
    return (count > 0) ? (sum / count) : fallback;
}

} // namespace modular
