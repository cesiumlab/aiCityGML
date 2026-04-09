#pragma once

#include "citygml/core/citygml_base.h"

namespace citygml {

// ============================================================
// 前向声明
// ============================================================

class ADEComponent;
class ExternalReference;

// ============================================================
// GenericAttribute - 泛化属性
// ============================================================

class GenericAttribute {
public:
    virtual ~GenericAttribute() = default;
    const std::string& getName() const { return name_; }
    virtual std::string getValueAsString() const = 0;
    virtual int getType() const = 0;
    
protected:
    std::string name_;
};

enum { ATTR_STRING = 0, ATTR_INT = 1, ATTR_DOUBLE = 2, ATTR_URI = 3 };

class GenericAttributeString : public GenericAttribute {
public:
    GenericAttributeString(const std::string& name, const std::string& value)
        : value_(value) { name_ = name; }
    const std::string& getValue() const { return value_; }
    std::string getValueAsString() const override { return value_; }
    int getType() const override { return ATTR_STRING; }
private:
    std::string value_;
};

class GenericAttributeInt : public GenericAttribute {
public:
    GenericAttributeInt(const std::string& name, int value)
        : value_(value) { name_ = name; }
    int getValue() const { return value_; }
    std::string getValueAsString() const override { return std::to_string(value_); }
    int getType() const override { return ATTR_INT; }
private:
    int value_;
};

class GenericAttributeDouble : public GenericAttribute {
public:
    GenericAttributeDouble(const std::string& name, double value)
        : value_(value) { name_ = name; }
    double getValue() const { return value_; }
    std::string getValueAsString() const override { return std::to_string(value_); }
    int getType() const override { return ATTR_DOUBLE; }
private:
    double value_;
};

class GenericAttributeURI : public GenericAttribute {
public:
    GenericAttributeURI(const std::string& name, const std::string& value)
        : value_(value) { name_ = name; }
    const std::string& getValue() const { return value_; }
    std::string getValueAsString() const override { return value_; }
    int getType() const override { return ATTR_URI; }
private:
    std::string value_;
};

using GenericAttributePtr = std::shared_ptr<GenericAttribute>;

// ============================================================
// ExternalReference - 外部引用
// ============================================================

class ExternalReference {
public:
    std::string getTargetResource() const { return targetResource_; }
    std::optional<std::string> getInformationSystem() const { return informationSystem_; }
    void setTargetResource(const std::string& uri) { targetResource_ = uri; }
    void setInformationSystem(const std::string& uri) { informationSystem_ = uri; }
    const std::string& getId() const { return id_; }
    void setId(const std::string& id) { id_ = id; }
    
private:
    std::string id_;
    std::string targetResource_;
    std::optional<std::string> informationSystem_;
};

using ExternalReferencePtr = std::shared_ptr<ExternalReference>;

// ============================================================
// CityGMLObject - 基础对象
// ============================================================

class CityGMLObject {
public:
    virtual ~CityGMLObject() = default;
    
    const std::string& getId() const { return id_; }
    void setId(const std::string& id) { id_ = id; }
    
private:
    std::string id_;
};

// ============================================================
// CityGMLFeatureWithLifespan - 带时间跨度的特征
// ============================================================

class CityGMLFeatureWithLifespan : public CityGMLObject {
public:
    const std::optional<DateTime>& getCreationDate() const { return creationDate_; }
    void setCreationDate(const DateTime& date) { creationDate_ = date; }
    
    const std::optional<DateTime>& getValidFrom() const { return validFrom_; }
    void setValidFrom(const DateTime& date) { validFrom_ = date; }
    
    const std::optional<DateTime>& getValidTo() const { return validTo_; }
    void setValidTo(const DateTime& date) { validTo_ = date; }
    
    const std::optional<DateTime>& getTerminationDate() const { return terminationDate_; }
    void setTerminationDate(const DateTime& date) { terminationDate_ = date; }
    
protected:
    std::optional<DateTime> creationDate_;
    std::optional<DateTime> validFrom_;
    std::optional<DateTime> validTo_;
    std::optional<DateTime> terminationDate_;
};

// ============================================================
// CityGMLFeature - 城市特征对象
// ============================================================

class CityGMLFeature : public CityGMLFeatureWithLifespan {
public:
    virtual ~CityGMLFeature() = default;
    
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    const std::string& getDescription() const { return description_; }
    void setDescription(const std::string& desc) { description_ = desc; }
    
    const std::vector<ExternalReferencePtr>& getExternalReferences() const { return externalReferences_; }
    void addExternalReference(ExternalReferencePtr ref) { externalReferences_.push_back(ref); }
    
    const std::vector<GenericAttributePtr>& getGenericAttributes() const { return genericAttributes_; }
    void addGenericAttribute(GenericAttributePtr attr) { genericAttributes_.push_back(attr); }
    
private:
    std::string name_;
    std::string description_;
    std::vector<ExternalReferencePtr> externalReferences_;
    std::vector<GenericAttributePtr> genericAttributes_;
};

} // namespace citygml