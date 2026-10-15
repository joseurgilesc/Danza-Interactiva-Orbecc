#pragma once

#include "mapping/ParameterTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace modular {

class MappingLayer {
public:
    MappingLayer();

    const std::vector<ParameterBinding>& getAllMappings() const;
    std::vector<ParameterBinding>& getAllMappings();

    ParameterBinding* findMapping(const std::string& id);
    const ParameterBinding* findMapping(const std::string& id) const;

    std::string addMapping(ParameterBinding binding);
    bool removeMapping(const std::string& id);
    bool setMappingChannel(const std::string& id, const std::string& channel);
    bool setMappingDevice(const std::string& id, const std::string& device);
    bool setMappingEnabled(const std::string& id, bool enabled);

private:
    std::vector<ParameterBinding> mappings_;
    int idCounter_;
};

} // namespace modular
