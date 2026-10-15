#pragma once

#include "mapping/ParameterTypes.h"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace modular {

class ParameterRegistry {
public:
    static ParameterRegistry& getGlobal();

    bool registerParam(const Parameter& parameter);
    bool unregisterParam(const std::string& id);
    bool hasParam(const std::string& id) const;

    std::optional<Parameter> getParam(const std::string& id) const;
    std::vector<Parameter> getAllParams() const;
    std::vector<Parameter> getParamsByScope(ParameterScope scope) const;

    bool setValue(const std::string& id, float value);
    bool setValue(const std::string& id, const glm::vec2& value);
    bool setValue(const std::string& id, const glm::vec3& value);

    bool saveToJson(const std::string& filepath) const;
    bool loadFromJson(const std::string& filepath);

private:
    ParameterRegistry() = default;

    bool updateScalarValueLocked(Parameter& parameter, float value);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Parameter> parameters_;
};

} // namespace modular
