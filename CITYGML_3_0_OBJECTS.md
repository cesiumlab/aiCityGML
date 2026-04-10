# CityGML 3.0 对象模型

本项目基于 CityGML 3.0 官方 XSD Schema 定义，实现了完整的 C++ 对象模型。

## CityGML 3.0 规范来源

- **官方规范**: https://www.ogc.org/standards/citygml
- **Schema 文件**: http://schemas.opengis.net/citygml/3.0/
- **GitHub 仓库**: https://github.com/opengeospatial/CityGML-3.0Encodings

## 目录结构

```
include/citygml/
├── citygml.h                    # 主头文件，包含所有模块
├── citygml_pch.h               # 预编译头文件
├── core/                       # Core 模块（核心对象）
│   ├── citygml_base.h          # 基类和枚举
│   ├── citygml_feature.h       # 特征对象
│   ├── citygml_space.h         # 空间对象
│   ├── citygml_surface.h       # 表面对象
│   ├── citygml_appearance.h    # 外观对象
│   ├── citygml_geometry.h      # 几何对象
│   └── citygml_object.h        # 城市对象和模型
├── construction/               # Construction 模块
│   └── citygml_construction.h  # 建筑结构基类
└── building/                   # Building 模块
    └── citygml_building.h     # 建筑对象

src/
├── core/                       # Core 模块实现
│   ├── citygml_base.cpp
│   ├── citygml_feature.cpp
│   ├── citygml_space.cpp
│   ├── citygml_surface.cpp
│   ├── citygml_appearance.cpp
│   ├── citygml_geometry.cpp
│   └── citygml_object.cpp
├── construction/               # Construction 模块实现
│   └── citygml_construction.cpp
└── building/                   # Building 模块实现
    └── citygml_building.cpp
```

## 对象层次结构

### Core 模块

```
CityGMLObject (基类)
└── CityGMLFeature
    ├── CityGMLFeatureWithLifespan
    │   ├── AbstractCityObject (所有城市对象的基类)
    │   │   ├── AbstractSpace
    │   │   │   ├── AbstractLogicalSpace
    │   │   │   │   └── AbstractBuildingSubdivision
    │   │   │   │       ├── Storey
    │   │   │   │       └── BuildingUnit
    │   │   │   └── AbstractPhysicalSpace
    │   │   │       ├── AbstractOccupiedSpace
    │   │   │       │   ├── AbstractBuilding
    │   │   │       │   │   ├── Building
    │   │   │       │   │   ├── BuildingPart
    │   │   │       │   │   ├── BuildingConstructiveElement
    │   │   │       │   │   ├── BuildingInstallation
    │   │   │       │   │   └── BuildingFurniture
    │   │   │       │   └── (其他 OccupiedSpace 子类)
    │   │   │       └── AbstractUnoccupiedSpace
    │   │   │           └── BuildingRoom
    │   │   └── AbstractSpaceBoundary
    │   │       └── AbstractThematicSurface
    │   │           └── ClosureSurface
    │   ├── AbstractAppearance
    │   │   └── Appearance
    │   ├── AbstractDynamizer
    │   ├── AbstractVersion
    │   └── AbstractVersionTransition
    └── Address

CityModel (根容器)
```

### Building 模块

- **Building**: 建筑物（可包含 BuildingPart）
- **BuildingPart**: 建筑部件
- **BuildingRoom**: 建筑房间
- **BuildingConstructiveElement**: 建筑构件（墙、板、楼梯等）
- **BuildingInstallation**: 建筑设施（阳台、天线等）
- **BuildingFurniture**: 建筑家具
- **Storey**: 楼层
- **BuildingUnit**: 建筑单元（可销售/出租的单元）

## 未来扩展

### 其他模块

根据 CityGML 3.0 规范，还可以添加以下模块：

- **Transportation**: 交通网络
- **Vegetation**: 植被
- **WaterBody**: 水体
- **Bridge**: 桥梁
- **Tunnel**: 隧道
- **Relief**: 地形
- **LandUse**: 土地利用
- **CityFurniture**: 城市家具
- **GenericCityObject**: 通用城市对象

### ADE 扩展

ADE (Application Domain Extensions) 允许扩展 CityGML 模型以满足特定应用需求。每个 ADE 类都是对应抽象基类的钩子。

## 使用示例

```cpp
#include "citygml/citygml.h"

using namespace citygml;

// 创建建筑
auto building = std::make_shared<Building>();
building->setId("building_1");
building->setGmlId("GML_building_001");
building->setStoreysAboveGround(3);
building->setStoreysBelowGround(1);

// 添加建筑部件
auto part = std::make_shared<BuildingPart>();
building->addBuildingPart(part);

// 添加地址
auto addr = std::make_shared<Address>();
addr->setXalAddress("123 Main Street");
building->addAddress(addr);

// 创建城市模型
auto model = std::make_shared<CityModel>();
model->setId("city_model_1");
model->addCityObject(building);
```

## 编译

使用 CMake 编译：

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 许可

本项目遵循 CityGML 标准的 OGC 许可。
