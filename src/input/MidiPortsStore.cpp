#include "input/MidiPortsStore.h"

#include "ofMain.h"

namespace modular {

MidiPortsStore::MidiPortsStore()
    : idCounter_(0) {}

const std::vector<MidiPortInfo>& MidiPortsStore::getPorts() const {
    return ports_;
}

std::string MidiPortsStore::addPort(const std::string& name, const std::string& device, bool isVirtual, int channel) {
    MidiPortInfo info;
    info.id = "port_" + std::to_string(++idCounter_);
    info.name = name;
    info.device = device;
    info.isVirtual = isVirtual;
    info.channel = channel;
    ports_.push_back(info);
    return info.id;
}

bool MidiPortsStore::setEnabled(const std::string& id, bool enabled) {
    MidiPortInfo* port = findPort(id);
    if (!port) {
        ofLogWarning("MidiPortsStore") << "Attempted to toggle unknown port " << id;
        return false;
    }
    port->enabled = enabled;
    return true;
}

bool MidiPortsStore::setLearning(const std::string& id, bool learning) {
    MidiPortInfo* port = findPort(id);
    if (!port) {
        return false;
    }
    port->learning = learning;
    return true;
}

bool MidiPortsStore::setChannel(const std::string& id, int channel) {
    MidiPortInfo* port = findPort(id);
    if (!port) {
        return false;
    }
    port->channel = ofClamp(channel, 1, 16);
    return true;
}

MidiPortInfo* MidiPortsStore::findPort(const std::string& id) {
    for (auto& port : ports_) {
        if (port.id == id) {
            return &port;
        }
    }
    return nullptr;
}

} // namespace modular
