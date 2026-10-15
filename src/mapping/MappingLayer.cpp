#include "mapping/MappingLayer.h"

#include "ofMain.h"

#include <algorithm>

namespace modular {

MappingLayer::MappingLayer()
    : idCounter_(0) {}

const std::vector<ParameterBinding>& MappingLayer::getAllMappings() const {
    return mappings_;
}

std::vector<ParameterBinding>& MappingLayer::getAllMappings() {
    return mappings_;
}

ParameterBinding* MappingLayer::findMapping(const std::string& id) {
    for (auto& binding : mappings_) {
        if (binding.bindingId == id) {
            return &binding;
        }
    }
    return nullptr;
}

const ParameterBinding* MappingLayer::findMapping(const std::string& id) const {
    for (const auto& binding : mappings_) {
        if (binding.bindingId == id) {
            return &binding;
        }
    }
    return nullptr;
}

std::string MappingLayer::addMapping(ParameterBinding binding) {
    if (binding.bindingId.empty()) {
        binding.bindingId = "mapping_" + std::to_string(++idCounter_);
    }
    mappings_.push_back(binding);
    return binding.bindingId;
}

bool MappingLayer::removeMapping(const std::string& id) {
    auto it = std::remove_if(mappings_.begin(), mappings_.end(), [&](const ParameterBinding& binding) {
        return binding.bindingId == id;
    });
    if (it == mappings_.end()) {
        return false;
    }
    mappings_.erase(it, mappings_.end());
    return true;
}

bool MappingLayer::setMappingChannel(const std::string& id, const std::string& channel) {
    ParameterBinding* binding = findMapping(id);
    if (!binding) {
        return false;
    }
    binding->sourceChannel = channel;
    return true;
}

bool MappingLayer::setMappingDevice(const std::string& id, const std::string& device) {
    ParameterBinding* binding = findMapping(id);
    if (!binding) {
        return false;
    }
    binding->inputDevice = device;
    return true;
}

bool MappingLayer::setMappingEnabled(const std::string& id, bool enabled) {
    ParameterBinding* binding = findMapping(id);
    if (!binding) {
        return false;
    }
    binding->enabled = enabled;
    return true;
}

} // namespace modular
