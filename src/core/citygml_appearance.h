#pragma once

#include "core/citygml_feature.h"

namespace citygml {

// ============================================================
// TextureCoordinates - 纹理坐标
// ============================================================

struct TextureCoordinates {
    std::string ringId;
    std::vector<std::array<double, 2>> coordinates;
};

// ============================================================
// AbstractSurfaceData - 表面数据基类
// ============================================================

class AbstractSurfaceData : public CityGMLObject {
public:
    virtual ~AbstractSurfaceData() = default;
    
    const std::optional<std::string>& getTheme() const { return theme_; }
    void setTheme(const std::string& theme) { theme_ = theme; }
    
protected:
    std::optional<std::string> theme_;
};

using SurfaceDataPtr = std::shared_ptr<AbstractSurfaceData>;

// ============================================================
// X3DMaterial - X3D 材质
// ============================================================

class X3DMaterial : public AbstractSurfaceData {
public:
    const std::array<double, 4>& getDiffuseColor() const { return diffuseColor_; }
    void setDiffuseColor(double r, double g, double b, double a = 1.0) { diffuseColor_ = {r, g, b, a}; }

    const std::array<double, 4>& getAmbientColor() const { return ambientColor_; }
    void setAmbientColor(double r, double g, double b, double a = 1.0) { ambientColor_ = {r, g, b, a}; }

    const std::array<double, 4>& getSpecularColor() const { return specularColor_; }
    void setSpecularColor(double r, double g, double b, double a = 1.0) { specularColor_ = {r, g, b, a}; }

    const std::array<double, 4>& getEmissiveColor() const { return emissiveColor_; }
    void setEmissiveColor(double r, double g, double b, double a = 1.0) { emissiveColor_ = {r, g, b, a}; }

    double getTransparency() const { return transparency_; }
    void setTransparency(double t) { transparency_ = t; }

    double getShininess() const { return shininess_; }
    void setShininess(double s) { shininess_ = s; }

    bool getIsSmooth() const { return isSmooth_; }
    void setIsSmooth(bool smooth) { isSmooth_ = smooth; }

    const std::vector<std::string>& getTargets() const { return targets_; }
    void addTarget(const std::string& target) { targets_.push_back(target); }

private:
    std::array<double, 4> diffuseColor_ = {0.8, 0.8, 0.8, 1.0};
    std::array<double, 4> ambientColor_ = {0.2, 0.2, 0.2, 1.0};
    std::array<double, 4> specularColor_ = {0.0, 0.0, 0.0, 1.0};
    std::array<double, 4> emissiveColor_ = {0.0, 0.0, 0.0, 1.0};
    double transparency_ = 0.0;
    double shininess_ = 0.0;
    bool isSmooth_ = false;
    std::vector<std::string> targets_;
};

using X3DMaterialPtr = std::shared_ptr<X3DMaterial>;

// ============================================================
// ParameterizedTexture - 参数化纹理
// ============================================================

class ParameterizedTexture : public AbstractSurfaceData {
public:
    struct TextureTarget {
        std::string uri;  // XLink 引用，如 "#ringID"
        std::vector<TextureCoordinates> textureCoords; // 每个 ring 对应的纹理坐标
    };

    const std::string& getImageURI() const { return imageURI_; }
    void setImageURI(const std::string& uri) { imageURI_ = uri; }

    const std::string& getMimeType() const { return mimeType_; }
    void setMimeType(const std::string& type) { mimeType_ = type; }

    const std::vector<TextureTarget>& getTargets() const { return targets_; }
    void addTarget(const TextureTarget& target) { targets_.push_back(target); }

private:
    std::string imageURI_;
    std::string mimeType_;
    std::vector<TextureTarget> targets_;
    std::vector<TextureCoordinates> textureCoordinates_;
};

using ParameterizedTexturePtr = std::shared_ptr<ParameterizedTexture>;

// ============================================================
// AbstractAppearance - 外观基类
// ============================================================

class AbstractAppearance : public CityGMLObject {
public:
    virtual ~AbstractAppearance() = default;
    
    const std::optional<std::string>& getTheme() const { return theme_; }
    void setTheme(const std::string& theme) { theme_ = theme; }
    
    const std::vector<SurfaceDataPtr>& getSurfaceData() const { return surfaceData_; }
    void addSurfaceData(SurfaceDataPtr data) { surfaceData_.push_back(data); }
    
protected:
    std::optional<std::string> theme_;
    std::vector<SurfaceDataPtr> surfaceData_;
};

using AppearancePtr = std::shared_ptr<AbstractAppearance>;

} // namespace citygml