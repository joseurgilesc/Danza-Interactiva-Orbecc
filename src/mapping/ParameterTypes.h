#pragma once

#include "ofMain.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace modular {

// ============================================================================
// Parameter Types - tipos de parámetros del sistema
// ============================================================================

enum class ParameterType : uint8_t {
    FLOAT,
    INT,
    BOOL,
    ENUM,
    TRIGGER,
    VEC2,
    VEC3
};

enum class ParameterScope : uint8_t {
    GLOBAL,
    SCENE
};

enum class TransformType : uint8_t {
    LINEAR,
    EXPONENTIAL,
    LOGARITHMIC,
    BEZIER_CURVE,
    STEP,
    MAP,
    SCALE,
    OFFSET,
    INVERT,
    CLAMP,
    SMOOTH,
    LERP,
    CURVE_EASE_IN,
    CURVE_EASE_OUT,
    CURVE_EASE_IN_OUT
};

enum class QuantizationType : uint8_t {
    NONE,
    INTEGER,
    STEP_2,
    STEP_4,
    STEP_8,
    STEP_16,
    BEAT,
    BAR,
    FRAME
};

// ============================================================================
// Transform - transformación para bindings
// ============================================================================

struct Transform {
    TransformType type;
    std::vector<float> params;
    std::string curve;
    
    Transform() : type(TransformType::LINEAR) {}
};

// ============================================================================
// ParameterBinding - vínculo entre parámetros source y target
// ============================================================================

struct ParameterBinding {
    std::string bindingId;
    std::string sourceSceneId;
    std::string sourceParameterId;
    std::string inputDevice;
    std::string sourceChannel;
    std::string targetSceneId;
    std::string targetParameterId;
    Transform transform;
    bool enabled;
    float smoothing;
    QuantizationType quantization;
    
    ParameterBinding()
        : enabled(true)
        , smoothing(0.0f)
        , quantization(QuantizationType::NONE) {}
};

// ============================================================================
// Parameter - estructura completa de parámetro (16 campos)
// ============================================================================

struct Parameter {
    // Identificación (2 campos)
    std::string id;
    std::string label;
    
    // Tipo y scope (2 campos)
    ParameterType type;
    ParameterScope scope;
    
    // Rango y valores (6 campos)
    float min;
    float max;
    float defaultValue;
    float currentValue;
    float step;
    std::string unit;
    
    // Metadatos (6 campos)
    std::string description;
    std::string group;
    bool hidden;
    bool writable;
    bool hotReloadable;
    float smoothing;
    
    // Para ENUM type
    std::vector<std::string> enumValues;
    
    // Para VEC2/VEC3 type
    glm::vec2 vec2Value;
    glm::vec3 vec3Value;
    glm::vec2 vec2Default;
    glm::vec3 vec3Default;
    
    Parameter()
        : type(ParameterType::FLOAT)
        , scope(ParameterScope::SCENE)
        , min(0.0f)
        , max(1.0f)
        , defaultValue(0.5f)
        , currentValue(0.5f)
        , step(0.01f)
        , description("")
        , group("")
        , hidden(false)
        , writable(true)
        , hotReloadable(true)
        , smoothing(0.0f)
        , vec2Value(0.0f)
        , vec3Value(0.0f)
        , vec2Default(0.0f)
        , vec3Default(0.0f) {}
};

// ============================================================================
// Preset - configuración de valores para una escena
// ============================================================================

struct Preset {
    std::string id;
    std::string name;
    std::string description;
    std::vector<Parameter> parameters;
    std::string sceneId;
    bool isDefault;
};

// ============================================================================
// Suggested Mapping - sugerencia de mapeo para UI
// ============================================================================

struct SuggestedMapping {
    std::string parameterId;
    std::string sourceType;
    std::string channel;
    std::string control;
    float minInput;
    float maxInput;
    bool bidirectional;
    std::string description;
};

} // namespace modular
