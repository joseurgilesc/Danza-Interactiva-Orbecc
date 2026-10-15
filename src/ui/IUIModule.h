#pragma once

#include <string>

namespace modular {

class IUIModule {
public:
    virtual ~IUIModule() = default;

    virtual void setup() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void draw() = 0;

    virtual void setVisible(bool visible) = 0;
    virtual bool isVisible() const = 0;

    virtual std::string getPanelId() const = 0;
};

} // namespace modular
