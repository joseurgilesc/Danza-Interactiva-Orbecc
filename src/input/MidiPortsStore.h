#pragma once

#include <string>
#include <vector>

namespace modular {

struct MidiPortInfo {
    std::string id;
    std::string name;
    std::string device;
    bool enabled = true;
    bool isVirtual = false;
    bool learning = false;
    int channel = 1;
};

class MidiPortsStore {
public:
    MidiPortsStore();

    const std::vector<MidiPortInfo>& getPorts() const;

    std::string addPort(const std::string& name, const std::string& device, bool isVirtual, int channel);
    bool setEnabled(const std::string& id, bool enabled);
    bool setLearning(const std::string& id, bool learning);
    bool setChannel(const std::string& id, int channel);

private:
    MidiPortInfo* findPort(const std::string& id);

    std::vector<MidiPortInfo> ports_;
    int idCounter_;
};

} // namespace modular
