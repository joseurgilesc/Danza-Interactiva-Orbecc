#include "registry/ParameterRegistry.h"

#include "mapping/ParameterTypes.h"
#include "ofMain.h"
#include "ofUtils.h"

namespace modular {

ParameterRegistry& ParameterRegistry::getGlobal() {
    static ParameterRegistry instance;
    return instance;
}

bool ParameterRegistry::registerParam(const Parameter& parameter) {
    if (parameter.id.empty()) {
        ofLogWarning("ParameterRegistry") << "Attempted to register parameter without id";
        return false;
    }

    std::lock_guard<std::mutex> guard(mutex_);
    auto [it, inserted] = parameters_.insert({parameter.id, parameter});
    if (!inserted) {
        it->second = parameter;
    }
    return true;
}

bool ParameterRegistry::unregisterParam(const std::string& id) {
    if (id.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> guard(mutex_);
    return parameters_.erase(id) > 0;
}

bool ParameterRegistry::hasParam(const std::string& id) const {
    std::lock_guard<std::mutex> guard(mutex_);
    return parameters_.find(id) != parameters_.end();
}

std::optional<Parameter> ParameterRegistry::getParam(const std::string& id) const {
    std::lock_guard<std::mutex> guard(mutex_);
    auto it = parameters_.find(id);
    if (it == parameters_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<Parameter> ParameterRegistry::getAllParams() const {
    std::vector<Parameter> result;
    std::lock_guard<std::mutex> guard(mutex_);
    result.reserve(parameters_.size());
    for (const auto& entry : parameters_) {
        result.push_back(entry.second);
    }
    return result;
}

std::vector<Parameter> ParameterRegistry::getParamsByScope(ParameterScope scope) const {
    std::vector<Parameter> result;
    std::lock_guard<std::mutex> guard(mutex_);
    for (const auto& entry : parameters_) {
        if (entry.second.scope == scope) {
            result.push_back(entry.second);
        }
    }
    return result;
}

bool ParameterRegistry::setValue(const std::string& id, float value) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto it = parameters_.find(id);
    if (it == parameters_.end()) {
        return false;
    }
    return updateScalarValueLocked(it->second, value);
}

bool ParameterRegistry::setValue(const std::string& id, const glm::vec2& value) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto it = parameters_.find(id);
    if (it == parameters_.end()) {
        return false;
    }
    if (it->second.type != ParameterType::VEC2) {
        return false;
    }
    it->second.vec2Value = value;
    return true;
}

bool ParameterRegistry::setValue(const std::string& id, const glm::vec3& value) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto it = parameters_.find(id);
    if (it == parameters_.end()) {
        return false;
    }
    if (it->second.type != ParameterType::VEC3) {
        return false;
    }
    it->second.vec3Value = value;
    return true;
}

bool ParameterRegistry::updateScalarValueLocked(Parameter& parameter, float value) {
    if (!parameter.writable) {
        return false;
    }
    parameter.currentValue = value;
    return true;
}

bool ParameterRegistry::saveToJson(const std::string& filepath) const {
    std::lock_guard<std::mutex> guard(mutex_);
    ofJson json;
    
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        ofJson paramJson;
        paramJson["id"] = param.id;
        paramJson["label"] = param.label;
        
        // Save value based on type
        switch (param.type) {
            case ParameterType::FLOAT:
            case ParameterType::INT:
            case ParameterType::BOOL:
                paramJson["value"] = param.currentValue;
                break;
            case ParameterType::VEC2:
                paramJson["value"] = {param.vec2Value.x, param.vec2Value.y};
                break;
            case ParameterType::VEC3:
                paramJson["value"] = {param.vec3Value.x, param.vec3Value.y, param.vec3Value.z};
                break;
            default:
                continue;
        }
        
        json["parameters"].push_back(paramJson);
    }
    
    // Also save scene-specific values (not just registry state)
    // This ensures new parameters get saved too
    
    bool success = ofSaveJson(filepath, json);
    if (success) {
        ofLogNotice("ParameterRegistry") << "Saved parameters to: " << filepath;
    } else {
        ofLogError("ParameterRegistry") << "Failed to save parameters to: " << filepath;
    }
    return success;
}

bool ParameterRegistry::loadFromJson(const std::string& filepath) {
    ofJson json = ofLoadJson(filepath);
    if (json.is_null()) {
        ofLogError("ParameterRegistry") << "Failed to load parameters from: " << filepath;
        return false;
    }
    
    std::lock_guard<std::mutex> guard(mutex_);
    
    int loaded = 0;
    int notFound = 0;
    
    if (json.contains("parameters")) {
        for (const auto& paramJson : json["parameters"]) {
            std::string id = paramJson.value("id", "");
            if (id.empty()) continue;
            
            auto it = parameters_.find(id);
            if (it == parameters_.end()) {
                ofLogWarning("ParameterRegistry") << "Param not found in registry: " << id;
                notFound++;
                continue;
            }
            
            auto& param = it->second;
            if (!param.writable) continue;
            
            if (paramJson.contains("value")) {
                const auto& value = paramJson["value"];
                float oldValue = param.currentValue;
                if ((param.type == ParameterType::FLOAT || param.type == ParameterType::INT || param.type == ParameterType::BOOL) && value.is_number()) {
                    param.currentValue = value.get<float>();
                    ofLogNotice("ParameterRegistry") << "Loaded " << id << ": " << oldValue << " -> " << param.currentValue;
                    loaded++;
                } else if (param.type == ParameterType::VEC2 && value.is_array() && value.size() >= 2) {
                    param.vec2Value = glm::vec2(value[0].get<float>(), value[1].get<float>());
                    loaded++;
                } else if (param.type == ParameterType::VEC3 && value.is_array() && value.size() >= 3) {
                    param.vec3Value = glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
                    loaded++;
                }
            }
        }
    }
    
    ofLogNotice("ParameterRegistry") << "Loaded " << loaded << " params, " << notFound << " not found from: " << filepath;
    return true;
}

} // namespace modular
