#include "citygml/citygml_parser.h"
#include "citygml/parser/xml_document.h"
#include "citygml/parser/gml_geometry_parser.h"
#include "citygml/parser/citygml_reader.h"
#include "citygml/parser/gml_envelope_parser.h"
#include "citygml/core/citygml_appearance.h"
#include "citygml/core/citygml_geometry.h"

#include <map>
#include <set>

namespace citygml {

CityGMLParser::CityGMLParser()
    : context_(std::make_shared<ParserContext>())
    , geometryParser_(std::make_shared<GMLGeometryParser>(context_))
    , cityGMLReader_(std::make_shared<CityGMLReader>(context_, geometryParser_))
{
}

CityGMLParser::~CityGMLParser() = default;

ParseResult CityGMLParser::parse(const std::string& filePath, std::shared_ptr<CityModel>& cityModel, const ParseOptions& options) {
    if (filePath.empty()) {
        return ParseResult::error("Empty file path");
    }

    XMLDocument doc;
    if (!doc.loadFile(filePath.c_str())) {
        return ParseResult::error(std::string("XML parse failed: ") + doc.errorStr());
    }

    void* root = doc.root();
    if (!root) {
        return ParseResult::error("No root node");
    }

    auto result = initializeParser(filePath);
    if (!result.success) {
        return result;
    }

    return parseCityModelNode(root, cityModel);
}

ParseResult CityGMLParser::parseString(const std::string& xmlContent, std::shared_ptr<CityModel>& cityModel, const ParseOptions& options) {
    if (xmlContent.empty()) {
        return ParseResult::error("Empty XML content");
    }

    XMLDocument doc;
    if (!doc.parse(xmlContent.c_str())) {
        return ParseResult::error(std::string("XML parse failed: ") + doc.errorStr());
    }

    void* root = doc.root();
    if (!root) {
        return ParseResult::error("No root node");
    }

    context_ = std::make_shared<ParserContext>();
    geometryParser_ = std::make_shared<GMLGeometryParser>(context_);
    cityGMLReader_ = std::make_shared<CityGMLReader>(context_, geometryParser_);

    return parseCityModelNode(root, cityModel);
}

ParseResult CityGMLParser::initializeParser(const std::string& filePath) {
    context_ = std::make_shared<ParserContext>();
    geometryParser_ = std::make_shared<GMLGeometryParser>(context_);
    cityGMLReader_ = std::make_shared<CityGMLReader>(context_, geometryParser_);

    return ParseResult::successResult();
}

ParseResult CityGMLParser::parseCityModelNode(void* node, std::shared_ptr<CityModel>& cityModel) {
    cityModel = std::make_shared<CityModel>();

    std::string gmlId = XMLDocument::attribute(node, "gml:id");
    if (!gmlId.empty()) {
        cityModel->setId(gmlId);
    }

    void* nameNode = XMLDocument::child(node, "name");
    if (!nameNode) nameNode = XMLDocument::child(node, "gml:name");
    if (nameNode) {
        cityModel->setName(XMLDocument::text(nameNode));
    }

    void* boundedBy = XMLDocument::child(node, "boundedBy");
    if (!boundedBy) boundedBy = XMLDocument::child(node, "gml:boundedBy");
    if (!boundedBy) boundedBy = XMLDocument::child(node, "core:boundedBy");

    if (boundedBy) {
        GMLEnvelopeParser envelopeParser;
        auto envelope = envelopeParser.parse(boundedBy, context_);
        if (envelope) {
            cityModel->setEnvelope(*envelope);
        }
    }

    std::vector<void*> appearanceMembers = XMLDocument::children(node, "appearanceMember");
    if (appearanceMembers.empty()) {
        appearanceMembers = XMLDocument::children(node, "app:appearanceMember");
    }
    for (size_t i = 0; i < appearanceMembers.size(); ++i) {
        void* appNode = XMLDocument::firstChildElement(appearanceMembers[i]);
        if (appNode) {
            std::string appName = XMLDocument::nodeName(appNode);
            if (appName == "Appearance" || appName == "app:Appearance") {
                if (auto appearance = parseAppearance(appNode)) {
                    cityModel->addAppearance(appearance);
                }
            }
        }
    }

    std::vector<void*> memberList = XMLDocument::children(node, "cityObjectMember");
    if (memberList.empty()) {
        memberList = XMLDocument::children(node, "core:cityObjectMember");
    }

    for (size_t i = 0; i < memberList.size(); ++i) {
        void* objNode = XMLDocument::firstChildElement(memberList[i]);
        if (objNode) {
            auto cityObject = cityGMLReader_->readCityObject(objNode);
            if (cityObject) {
                cityModel->addCityObject(cityObject);
            }
        }
    }

    // Step 2: Build polygon ID map for bidirectional association
    std::map<std::string, std::shared_ptr<citygml::Polygon>> polygonMap;
    for (const auto& cityObj : cityModel->getCityObjects()) {
        // LOD 0
        if (cityObj->getLod0FootPrint()) collectPolygons(cityObj->getLod0FootPrint(), polygonMap);
        if (cityObj->getLod0RoofEdge()) collectPolygons(cityObj->getLod0RoofEdge(), polygonMap);
        if (cityObj->getLod0MultiSurface()) collectPolygons(cityObj->getLod0MultiSurface(), polygonMap);
        // LOD 1
        if (cityObj->getLod1MultiSurface()) collectPolygons(cityObj->getLod1MultiSurface(), polygonMap);
        if (cityObj->getLod1Solid()) collectPolygons(cityObj->getLod1Solid(), polygonMap);
        // LOD 2
        if (cityObj->getLod2MultiSurface()) collectPolygons(cityObj->getLod2MultiSurface(), polygonMap);
        if (cityObj->getLod2Solid()) collectPolygons(cityObj->getLod2Solid(), polygonMap);
        // LOD 3
        if (cityObj->getLod3MultiSurface()) collectPolygons(cityObj->getLod3MultiSurface(), polygonMap);
        if (cityObj->getLod3Solid()) collectPolygons(cityObj->getLod3Solid(), polygonMap);
        // LOD 4
        if (cityObj->getLod4MultiSurface()) collectPolygons(cityObj->getLod4MultiSurface(), polygonMap);
        if (cityObj->getLod4Solid()) collectPolygons(cityObj->getLod4Solid(), polygonMap);
    }

    // Step 3: Set appearance data for each polygon based on gml:id matching
    for (const auto& appearance : cityModel->getAppearances()) {
        for (const auto& sd : appearance->getSurfaceData()) {
            if (auto mat = std::dynamic_pointer_cast<citygml::X3DMaterial>(sd)) {
                for (const std::string& targetId : mat->getTargets()) {
                    std::string id = stripFragment(targetId);
                    auto it = polygonMap.find(id);
                    if (it != polygonMap.end()) {
                        it->second->setAppearance(mat);
                    }
                }
            } else if (auto tex = std::dynamic_pointer_cast<citygml::ParameterizedTexture>(sd)) {
                for (const auto& target : tex->getTargets()) {
                    std::string id = stripFragment(target.uri);
                    auto it = polygonMap.find(id);
                    if (it != polygonMap.end()) {
                        it->second->setAppearance(tex);
                    }
                }
            }
        }
    }

    return ParseResult::successResult();
}

void CityGMLParser::collectPolygons(
    std::shared_ptr<citygml::AbstractGeometry> geom,
    std::map<std::string, std::shared_ptr<citygml::Polygon>>& polygonMap) {
    if (!geom) return;

    if (auto polygon = std::dynamic_pointer_cast<citygml::Polygon>(geom)) {
        std::string pid = polygon->getId();
        if (!pid.empty()) polygonMap[pid] = polygon;
    } else if (auto ms = std::dynamic_pointer_cast<citygml::MultiSurface>(geom)) {
        for (size_t i = 0; i < ms->getGeometriesCount(); ++i) {
            collectPolygons(ms->getGeometry(i), polygonMap);
        }
    } else if (auto solid = std::dynamic_pointer_cast<citygml::Solid>(geom)) {
        if (auto shell = solid->getOuterShell()) {
            collectPolygons(shell, polygonMap);
        }
    } else if (auto multiSolid = std::dynamic_pointer_cast<citygml::MultiSolid>(geom)) {
        for (size_t i = 0; i < multiSolid->getGeometriesCount(); ++i) {
            collectPolygons(multiSolid->getGeometry(i), polygonMap);
        }
    }
}

std::string CityGMLParser::stripFragment(const std::string& uri) const {
    size_t pos = uri.find('#');
    if (pos != std::string::npos) {
        return uri.substr(pos + 1);
    }
    return uri;
}

AppearancePtr CityGMLParser::parseAppearance(void* node) {
    auto appearance = std::make_shared<AbstractAppearance>();

    std::string gmlId = XMLDocument::attribute(node, "gml:id");
    if (!gmlId.empty()) {
        appearance->setId(gmlId);
    }

    void* themeNode = XMLDocument::child(node, "theme");
    if (!themeNode) themeNode = XMLDocument::child(node, "app:theme");
    if (themeNode) {
        appearance->setTheme(XMLDocument::text(themeNode));
    }

    std::vector<void*> surfaceDataMembers = XMLDocument::children(node, "surfaceDataMember");
    if (surfaceDataMembers.empty()) {
        surfaceDataMembers = XMLDocument::children(node, "app:surfaceDataMember");
    }

    for (size_t i = 0; i < surfaceDataMembers.size(); ++i) {
        void* sdNode = XMLDocument::firstChildElement(surfaceDataMembers[i]);
        if (!sdNode) continue;

        std::string sdName = XMLDocument::nodeName(sdNode);
        if (sdName == "X3DMaterial" || sdName == "app:X3DMaterial") {
            auto material = parseX3DMaterial(sdNode);
            if (material) appearance->addSurfaceData(material);
        }
        else if (sdName == "ParameterizedTexture" || sdName == "app:ParameterizedTexture") {
            auto texture = parseParameterizedTexture(sdNode);
            if (texture) appearance->addSurfaceData(texture);
        }
    }

    return appearance;
}

X3DMaterialPtr CityGMLParser::parseX3DMaterial(void* node) {
    auto material = std::make_shared<X3DMaterial>();

    std::string gmlId = XMLDocument::attribute(node, "gml:id");
    if (!gmlId.empty()) {
        material->setId(gmlId);
    }

    auto targetNodes = XMLDocument::children(node, "target");
    if (targetNodes.empty()) targetNodes = XMLDocument::children(node, "app:target");
    for (void* targetNode : targetNodes) {
        std::string target = XMLDocument::text(targetNode);
        if (!target.empty()) material->addTarget(target);
    }

    void* diffuseNode = XMLDocument::child(node, "diffuseColor");
    if (!diffuseNode) diffuseNode = XMLDocument::child(node, "app:diffuseColor");
    if (diffuseNode) {
        std::array<double, 4> c = parseColor(XMLDocument::text(diffuseNode));
        material->setDiffuseColor(c[0], c[1], c[2], c[3]);
    }

    void* ambientNode = XMLDocument::child(node, "ambientColor");
    if (!ambientNode) ambientNode = XMLDocument::child(node, "app:ambientColor");
    if (ambientNode) {
        std::array<double, 4> c = parseColor(XMLDocument::text(ambientNode));
        material->setAmbientColor(c[0], c[1], c[2], c[3]);
    }

    void* specularNode = XMLDocument::child(node, "specularColor");
    if (!specularNode) specularNode = XMLDocument::child(node, "app:specularColor");
    if (specularNode) {
        std::array<double, 4> c = parseColor(XMLDocument::text(specularNode));
        material->setSpecularColor(c[0], c[1], c[2], c[3]);
    }

    void* emissiveNode = XMLDocument::child(node, "emissiveColor");
    if (!emissiveNode) emissiveNode = XMLDocument::child(node, "app:emissiveColor");
    if (emissiveNode) {
        std::array<double, 4> c = parseColor(XMLDocument::text(emissiveNode));
        material->setEmissiveColor(c[0], c[1], c[2], c[3]);
    }

    void* transNode = XMLDocument::child(node, "transparency");
    if (!transNode) transNode = XMLDocument::child(node, "app:transparency");
    if (transNode) {
        try { material->setTransparency(std::stod(XMLDocument::text(transNode))); } catch (...) {}
    }

    void* shineNode = XMLDocument::child(node, "shininess");
    if (!shineNode) shineNode = XMLDocument::child(node, "app:shininess");
    if (shineNode) {
        try { material->setShininess(std::stod(XMLDocument::text(shineNode))); } catch (...) {}
    }

    void* smoothNode = XMLDocument::child(node, "isSmooth");
    if (!smoothNode) smoothNode = XMLDocument::child(node, "app:isSmooth");
    if (smoothNode) {
        std::string val = XMLDocument::text(smoothNode);
        material->setIsSmooth(val == "true" || val == "1");
    }

    return material;
}

ParameterizedTexturePtr CityGMLParser::parseParameterizedTexture(void* node) {
    auto texture = std::make_shared<ParameterizedTexture>();

    std::string gmlId = XMLDocument::attribute(node, "gml:id");
    if (!gmlId.empty()) {
        texture->setId(gmlId);
    }

    void* imageNode = XMLDocument::child(node, "imageURI");
    if (!imageNode) imageNode = XMLDocument::child(node, "app:imageURI");
    if (imageNode) {
        texture->setImageURI(XMLDocument::text(imageNode));
    }

    void* mimeNode = XMLDocument::child(node, "mimeType");
    if (!mimeNode) mimeNode = XMLDocument::child(node, "app:mimeType");
    if (mimeNode) {
        texture->setMimeType(XMLDocument::text(mimeNode));
    }

    auto targetNodes = XMLDocument::children(node, "target");
    if (targetNodes.empty()) targetNodes = XMLDocument::children(node, "app:target");
    for (void* targetNode : targetNodes) {
        ParameterizedTexture::TextureTarget target;
        std::string uriAttr = XMLDocument::attribute(targetNode, "uri");
        if (uriAttr.empty()) uriAttr = XMLDocument::attribute(targetNode, "xlink:href");
        target.uri = uriAttr;

        void* texCoordList = XMLDocument::child(targetNode, "TexCoordList");
        if (!texCoordList) texCoordList = XMLDocument::child(targetNode, "app:TexCoordList");
        if (texCoordList) {
            auto texCoordNodes = XMLDocument::children(texCoordList, "textureCoordinates");
            if (texCoordNodes.empty()) texCoordNodes = XMLDocument::children(texCoordList, "app:textureCoordinates");
            for (void* tcNode : texCoordNodes) {
                std::string ringIdAttr = XMLDocument::attribute(tcNode, "ring");
                if (ringIdAttr.empty()) ringIdAttr = XMLDocument::attribute(tcNode, "app:ring");

                TextureCoordinates tc;
                tc.ringId = ringIdAttr;

                std::string coordsStr = XMLDocument::text(tcNode);
                std::istringstream iss(coordsStr);
                double v;
                while (iss >> v) {
                    tc.coordinates.push_back({v, 0.0});
                }
                if (tc.coordinates.size() % 2 == 0) {
                    for (size_t j = 0; j < tc.coordinates.size() / 2; ++j) {
                        tc.coordinates[j][1] = tc.coordinates[tc.coordinates.size() / 2 + j][0];
                    }
                    tc.coordinates.resize(tc.coordinates.size() / 2);
                }

                target.textureCoords.push_back(tc);
            }
        }

        texture->addTarget(target);
    }

    return texture;
}

std::array<double, 4> CityGMLParser::parseColor(const std::string& colorStr) {
    std::array<double, 4> c = {0.8, 0.8, 0.8, 1.0};
    std::istringstream iss(colorStr);
    int i = 0;
    double v;
    while (iss >> v && i < 4) {
        c[i++] = v;
    }
    return c;
}

} // namespace citygml
