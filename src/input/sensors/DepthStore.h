#pragma once

// ============================================================================
// DepthStore — acceso compartido al mapa de profundidad (silueta del cuerpo).
//
// El OrbbecAdapter publica aquí el depth alineado (submuestreado) cada frame.
// Las escenas consultan la silueta real: un píxel es "cuerpo" si su profundidad
// está en rango (0 < depth < umbral). Es el contorno verdadero píxel a píxel,
// no la aproximación del convex hull de las articulaciones.
// ============================================================================

#include "ofMain.h"
#include <cstdint>
#include <vector>
#include <glm/vec2.hpp>

namespace modular {

class DepthStore {
public:
    static DepthStore& instance() {
        static DepthStore s;
        return s;
    }

    // Publica el depth alineado a color (mm). Submuestrea a la rejilla interna.
    void update(const std::vector<uint16_t>& depth, int w, int h) {
        if (depth.empty() || w <= 0 || h <= 0) return;
        gridW_ = kGridW;
        gridH_ = kGridH;
        grid_.resize(gridW_ * gridH_);
        for (int gy = 0; gy < gridH_; gy++) {
            int sy = gy * h / gridH_;
            for (int gx = 0; gx < gridW_; gx++) {
                int sx = gx * w / gridW_;
                grid_[gy * gridW_ + gx] = depth[sy * w + sx];
            }
        }
        hasData_ = true;
    }

    // ¿El punto normalizado (0..1, espacio de color) es cuerpo?
    bool isBody(float nx, float ny) const {
        if (!hasData_ || grid_.empty()) return false;
        int gx = static_cast<int>(nx * (gridW_ - 1));
        int gy = static_cast<int>(ny * (gridH_ - 1));
        gx = ofClamp(gx, 0, gridW_ - 1);
        gy = ofClamp(gy, 0, gridH_ - 1);
        uint16_t d = grid_[gy * gridW_ + gx];
        return d > 0 && d < thresholdMM_;
    }

    uint16_t at(int gx, int gy) const {
        if (grid_.empty() || gx < 0 || gy < 0 || gx >= gridW_ || gy >= gridH_) return 0;
        return grid_[gy * gridW_ + gx];
    }

    // Puntos del contorno del cuerpo (células de borde) en coords normalizadas 0..1.
    std::vector<glm::vec2> contourPoints() const {
        std::vector<glm::vec2> pts;
        if (!hasData_ || grid_.empty()) return pts;
        for (int gy = 1; gy < gridH_ - 1; gy++) {
            for (int gx = 1; gx < gridW_ - 1; gx++) {
                if (!cellBody(gx, gy, thresholdMM_)) continue;
                if (!cellBody(gx - 1, gy, thresholdMM_) || !cellBody(gx + 1, gy, thresholdMM_) ||
                    !cellBody(gx, gy - 1, thresholdMM_) || !cellBody(gx, gy + 1, thresholdMM_)) {
                    pts.push_back(glm::vec2(
                        static_cast<float>(gx) / (gridW_ - 1),
                        static_cast<float>(gy) / (gridH_ - 1)));
                }
            }
        }
        return pts;
    }

    bool hasData() const { return hasData_; }
    int gridW() const { return gridW_; }
    int gridH() const { return gridH_; }

    void setThreshold(uint16_t t) { thresholdMM_ = t; }
    uint16_t getThreshold() const { return thresholdMM_; }

    static constexpr int kGridW = 96;
    static constexpr int kGridH = 54;

private:
    DepthStore() = default;

    bool cellBody(int gx, int gy, uint16_t thresholdMM) const {
        uint16_t d = at(gx, gy);
        return d > 0 && d < thresholdMM;
    }

    std::vector<uint16_t> grid_;
    int gridW_ = 0, gridH_ = 0;
    bool hasData_ = false;
    uint16_t thresholdMM_ = 2500;   // umbral "es cuerpo" (mm)
};

} // namespace modular
