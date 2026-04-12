#pragma once

#include "core/citygml_feature.h"
#include "core/citygml_geometry.h"

namespace citygml {

// ============================================================
// AbstractOpening - 抽象开口基类（门、窗等）
// ============================================================

class AbstractOpening : public CityGMLFeature {
public:
    virtual ~AbstractOpening() = default;

    std::shared_ptr<MultiSurface> getMultiSurface() const { return multiSurface_; }
    void setMultiSurface(std::shared_ptr<MultiSurface> ms) { multiSurface_ = ms; }

private:
    std::shared_ptr<MultiSurface> multiSurface_;
};

using OpeningPtr = std::shared_ptr<AbstractOpening>;

// ============================================================
// Door - 门
// ============================================================

class Door : public AbstractOpening {
public:
    virtual ~Door() = default;

    const std::optional<std::string>& getAddress() const { return address_; }
    void setAddress(const std::string& addr) { address_ = addr; }

private:
    std::optional<std::string> address_;
};

using DoorPtr = std::shared_ptr<Door>;

// ============================================================
// Window - 窗户
// ============================================================

class Window : public AbstractOpening {
public:
    virtual ~Window() = default;
};

using WindowPtr = std::shared_ptr<Window>;

} // namespace citygml
