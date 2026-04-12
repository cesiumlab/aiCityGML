# CityGML 转 OBJ 转换器

> 本项目基于 Vibe Coding 方式生成。

一个纯 C++ 实现的 CityGML 1.0 / 2.0 / 3.0 数据解析与 Wavefront OBJ 格式生成工具。

## 功能特性

- **支持 CityGML 全模块**：建筑 (Building)、道路 (Road)、水体 (WaterBody)、植被 (Vegetation)、地形 (Relief)、桥梁 (Bridge)、隧道 (Tunnel) 等
- **支持多 LOD 级别**：LOD0 ~ LOD4，自动提取几何
- **多边形三角化**：基于 Earcut 算法，支持带孔洞多边形
- **自动法线计算**：面法线 + 顶点法线
- **材质颜色提取**：从 Appearance 模块提取 X3DMaterial 颜色
- **纹理支持**：ParameterizedTexture 引用关系（纹理图片路径输出到 MTL）
- **OBJ 格式输出**：支持 `.obj` + `.mtl` 材质库文件
- **零外部依赖**：核心依赖均为头文件库（TinyXML-2 + Earcut）

## 项目结构

```
citygml/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 本文件
├── README_en.md                # English version
├── src/                        # 源文件 + 头文件
│   ├── core/                   # 核心模块（Geometry、Surface、Space...）
│   ├── building/               # 建筑模块
│   ├── construction/           # 构造模块
│   ├── mesh/                   # 网格模块
│   ├── parser/                 # 解析器模块
│   ├── writers/                # OBJ 写入器
│   └── test/                   # 测试程序
├── cmake/                      # CMake 配置模板
└── testdata/                   # 测试数据
```

## 第三方依赖

| 库 | 版本要求 | 地址 | 说明 |
|---|---|---|---|
| **tinyxml2** | >= 9.0.0 | https://github.com/leethomason/tinyxml2 | XML 解析，C++ 头文件库 |
| **earcut.hpp** | 推荐 v2.2.4 | https://github.com/mapbox/earcut.hpp | 多边形三角化，单文件头文件库 |

> CMake 会通过 FetchContent 自动下载，无需手动准备。

## 构建说明

### 环境要求

- **C++ 编译器**：支持 C++17（MSVC 2019+ / GCC 9+ / Clang 12+）
- **CMake**：3.16 或更高版本
- **网络**：首次构建需联网下载 TinyXML-2 和 Earcut

### 快速构建（Windows / MSVC）

```bash
# 1. 进入项目目录
cd citygml

# 2. 配置 + 编译
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release --parallel

# 3. 运行测试
build\Release\test_parser.exe testdata\2.gml
```

### 快速构建（Linux / macOS）

```bash
cd citygml
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/test_parser testdata/2.gml
```

### 使用 CMake GUI 或 Visual Studio

直接用 CMake GUI 或 Visual Studio 打开项目根目录，设置源码目录和构建目录，点击 Configure 和 Generate 即可。

## 命令行工具用法

编译后在 `build/` 目录生成 `test_parser.exe`（Windows）或 `test_parser`（Linux/macOS）。

```bash
# 基本用法（自动生成同名 .obj 文件）
test_parser testdata/2.gml

# 指定输出文件
test_parser testdata/2.gml output/model.obj
```

## 编程接口

### 最简用法

```cpp
#include "parser/citygml_parser.h"
#include "mesh/mesh_generator.h"
#include "writers/obj_writer.h"

int main() {
    citygml::CityGMLParser parser;
    std::shared_ptr<citygml::CityModel> cityModel;

    citygml::ParseResult result = parser.parse("input/building.gml", cityModel);
    if (!result.success) {
        return 1;
    }

    citygml::MeshGenerator gen;
    std::vector<citygml::Mesh> allMeshes;

    for (const auto& obj : cityModel->getCityObjects()) {
        std::vector<citygml::Mesh> meshes;
        gen.triangulateCityObject(*obj, meshes);
        for (auto& m : meshes) {
            allMeshes.push_back(std::move(m));
        }
    }

    citygml::writeObjFile(allMeshes, "output/building.obj", "output/building.mtl");
    return 0;
}
```

### C++ API 速查表

| 类/函数 | 说明 |
|------|------|
| `CityGMLParser` | CityGML 解析器主类 |
| `parser.parse(path, model)` | 解析 CityGML 文件 |
| `parser.parseString(xml, model)` | 解析 XML 字符串 |
| `CityModel` | CityGML 城市模型根对象 |
| `CityModel::getCityObjects()` | 获取所有城市对象 |
| `MeshGenerator::triangulateCityObject()` | 将城市对象三角化生成 Mesh |
| `writeObjFile()` | 将 Mesh 写入 OBJ 文件 |

## 测试数据

项目 `testdata/` 目录已包含 CityGML 示例数据：

| 文件 | 说明 |
|------|------|
| `testdata/2.gml` | CityGML 2.0 建筑数据，LOD2 几何，包含外观纹理引用 |
| `testdata/2_appearance/*.png` | 纹理图片 |
| `testdata/FZK-Haus-LoD*.gml` | KIT 官方示例（LoD0 ~ LoD3） |
| `testdata/samplycity_*.gml` | Samply City 示例数据 |

### 其他推荐数据源

| 数据集 | 下载地址 | 说明 |
|--------|----------|------|
| **KIT CityGML 示例** | citygmlwiki.org | 学习入门 |
| **3D BAG（荷兰）** | 3dbag.nl | 大规模建筑数据 |
| **PLATEAU（日本）** | mlit.go.jp/plateau | 200+ 城市，官方开放 |
| **Random3Dcity** | github.com/tudelft3d/Random3Dcity | 程序化生成 |
| **柏林数据** | businesslocationcenter.de | LOD2 含纹理 |

## 示例输出

```bash
$ ./build/test_parser testdata/2.gml
Parsing: testdata/2.gml
Output OBJ: testdata/2.obj
Output MTL: testdata/2.mtl

[OK] Parse completed successfully.
Found 23 city objects.

Processing object 1/23: Building (id=BUILDING_0)
  -> 1 mesh(es), 24 vertices, 36 triangles.
Processing object 2/23: Building (id=BUILDING_1)
  -> 1 mesh(es), 20 vertices, 30 triangles.
...

============================================================
Exporting 23 mesh(es) to OBJ...
[OK] OBJ exported successfully.
  File: testdata/2.obj
  MTL:  testdata/2.mtl
  Total: 23 mesh(es), 1234 vertices, 5678 triangles.
============================================================
```

## 架构设计

```
CityGML XML ──▶ XML解析层 ──▶ CityGML读取器 ──▶ 内存模型
                  (TinyXML-2)       │              │
                                   ▼              ▼
                            XML事件回调    CityModel 树
                                            │
                                            ▼
                                   几何处理层 ──▶ OBJ写入器
                                    (Earcut)      (OBJ/MTL)
                                                   │
                                                   ▼
                                              .obj + .mtl
```

## 依赖库

| 库 | 版本 | 用途 | 许可证 | 集成方式 |
|----|------|------|--------|----------|
| TinyXML-2 | 9.0.0 | XML 解析 | Zlib | CMake FetchContent |
| Earcut | 2.2.4 | 多边形三角化 | ISC | CMake FetchContent |
| CMake | ≥3.16 | 构建系统 | BSD-3 | 系统工具 |

## 许可证

项目代码遵循 MIT 许可证。第三方依赖各自遵循其许可证（Zlib / ISC）。

## 已知限制

- 仅支持 CityGML 的几何数据，暂不支持拓扑关系（Topology）
- 隐式几何（ImplicitGeometry）的模板库引用需要外部提供模板文件
- 坐标参考系（CRS）转换需要额外的 proj 库支持（当前版本保留原始坐标）
- CityGML 3.0 支持待完善（Schema 层面支持，API 兼容）
