#pragma once

// ============================================================================
// Pendulum — péndulo multi-brazo (jerarquía), física de gravedad + amortiguación.
// Header-only (métodos inline) para compartir entre PendulumScene y MeshScene.
// ============================================================================

#include "ofMain.h"
#include <glm/vec2.hpp>
#include <cmath>
#include <memory>

namespace modular {

struct Pendulum {
    int hierarchy = 0;            // brazos restantes (0 = hoja)
    float size = 50.0f;
    float angle = 0.0f;
    float angularVel = 0.0f;
    float gravity = 0.094f;
    float damping = 0.998f;
    glm::vec2 origin{0.0f};
    glm::vec2 end{0.0f};
    std::shared_ptr<Pendulum> arm;

    Pendulum(float sz, int h, float g, float d)
        : hierarchy(h - 1), size(sz), gravity(g), damping(d), angle(ofRandom(glm::two_pi<float>())) {
        if (hierarchy > 0) {
            arm = std::make_shared<Pendulum>(sz / 1.5f, hierarchy, g, d);
        }
    }

    void update(float heading) {
        end = origin + glm::vec2(size * std::sin(angle), size * std::cos(angle));
        float accel = (-gravity / size) * std::sin(angle + heading);
        angle += angularVel;
        angularVel += accel;
        angularVel *= damping;
        if (arm) arm->update(heading);
    }

    glm::vec2 getTrail(const glm::vec2& offset) {
        if (arm) return arm->getTrail(offset) + end;
        return end + offset;
    }

    void draw() {
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
};

} // namespace modular
