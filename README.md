# CityGML 转 OBJ 转换器

基于Vibe Coding完成的 citygml解析器，主要特性：
1，兼容 citygml2.0 和 citygml3.0 标准
2，支持 各种几何体生成 三角化mesh
3，支持导出 obj格式的模型

一个纯 C++ 实现的 CityGML 1.0 / 2.0 / 3.0 数据解析与 Wavefront OBJ 格式生成工具。

## 功能特性

- **支持 CityGML 全模块**：建筑 (Building)、道路 (Road)、水体 (WaterBody)、植被 (Vegetation)、地形 (Relief)、桥梁 (Bridge)、隧道 (Tunnel) 等
- **支持多 LOD 级别**：LOD0 ~ LOD4，自动提取最高 LOD 几何
- **多边形三角化**：基于 Earcut 算法，支持带孔洞多边形
- **自动法线计算**：面法线 + 顶点法线
- **材质颜色提取**：从 Appearance 模块提取 X3DMaterial 颜色
- **纹理支持**：ParameterizedTexture 引用关系（纹理图片路径输出到 MTL）
- **OBJ 格式输出**：支持 `.obj` + `.mtl` 材质库文件
- **零外部依赖**：核心依赖均为头文件库（TinyXML-2 + Earcut）

## 项目结构

```
citygml-to-obj/
├── CMakeLists.txt              # CMake 构建配置
├── conanfile.txt               # Conan 依赖管理（可选）
├── README.md                   # 本文件
├── include/citygml/            # 头文件
│   ├── types.h                 # 数据类型定义
│   ├── geometry.h              # 几何计算工具
│   ├── xml_parser.h            # XML/GML 解析器
│   ├── citygml_reader.h        # CityGML 读取器
│   ├── geometry_processor.h    # 几何处理（三角化）
│   ├── obj_writer.h            # OBJ 文件写入器
│   └── citygml.h               # 主入口 API
├── src/                        # 源文件
│   ├── types.cpp
│   ├── geometry.cpp
│   ├── xml_parser.cpp
│   ├── citygml_reader.cpp      # 核心：CityGML 文档解析
│   ├── geometry_processor.cpp  # 核心：三角网格生成
│   ├── obj_writer.cpp         # 核心：OBJ/MTL 文件写入
│   ├── citygml_model.cpp
│   ├── citygml.cpp
│   └── main.cpp                # 命令行工具
├── tests/
│   └── test_converter.cpp      # 单元测试
└── testdata/                   # 测试数据
    └── 2.gml                   # CityGML 2.0 建筑示例（含纹理）
```

## 第三方依赖

| 库 | 版本要求 | 地址 | 说明 |
|---|---|---|---|
| **tinyxml2** | >= 9.0.0（推荐 9.0.0） | https://github.com/leethomason/tinyxml2 | XML 解析，C++ 源文件 tinyxml2.h + tinyxml2.cpp |
| **earcut.hpp** | 任意版本（推荐 v2.2.4） | https://github.com/mapbox/earcut.hpp | 多边形三角化，单文件头文件库 |

> CMake 会通过 FetchContent 自动下载，无需手动准备。网络不通时需配置 git 代理：
> `git config --global http.proxy http://127.0.0.1:7890`

## 构建说明

### 环境要求

- **C++ 编译器**：支持 C++17（MSVC 2019+ / GCC 9+ / Clang 12+）
- **CMake**：3.16 或更高版本
- **网络**：首次构建需联网下载 TinyXML-2 和 Earcut（通过 CMake FetchContent 自动下载）

### 快速构建（Windows / MSVC）

```bash
# 1. 进入项目目录
cd e:\opensource\citygml

# 2. 配置 + 编译（一行命令搞定）
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release --parallel

# 3. 安装（头文件 + lib）
cmake --install build --config Release

# 4. 运行测试
build\Release\test_parser.exe
```

### 快速构建（Linux / macOS）

```bash
cd e:/opensource/citygml
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build
./build/test_parser
```

### 使用 CMake GUI 或 Visual Studio

直接用 CMake GUI 或 Visual Studio 打开项目根目录，设置源码目录和构建目录，点击 Configure 和 Generate 即可。

## 命令行工具用法

编译后在 `build/` 目录生成 `citygml2obj.exe`（Windows）或 `citygml2obj`（Linux/macOS）。

```bash
# 基本用法（自动生成同名 .obj 文件）
citygml2obj testdata/2.gml

# 指定输出文件
citygml2obj testdata/2.gml output/model.obj

# 指定目标 LOD 级别
citygml2obj testdata/2.gml output.obj --lod 2

# 居中到原点（适合游戏引擎导入）
citygml2obj testdata/2.gml output.obj --center

# Blender 导出预设（翻转 YZ 轴 + 居中）
citygml2obj testdata/2.gml output.obj --blender

# Cesium 导出预设
citygml2obj testdata/2.gml output.obj --cesium

# 打印模型统计信息
citygml2obj testdata/2.gml --stats

# 查看帮助
citygml2obj --help
```

### 预设说明

| 预设 | 选项 | 适用场景 |
|------|------|----------|
| `--blender` | `--flip-yz --center` | Blender 导入 |
| `--cesium` | `--flip-yz --center --no-mtl` | Cesium 3D Tiles |
| 默认 | 无变换 | 通用查看器（MeshLab, 3D Viewer） |

## 编程接口

### 最简用法

```cpp
#include <citygml/citygml.h>

int main() {
    bool ok = citygml::convert("input/building.gml", "output/building.obj");
    return ok ? 0 : 1;
}
```

### 高级用法

```cpp
#include <citygml/citygml.h>

int main() {
    // 配置读取选项
    citygml::ReaderOptions readerOpts;
    readerOpts.targetLOD = citygml::LODLevel::LOD2;
    readerOpts.centerModel = true;

    // 加载模型
    auto model = citygml::load("testdata/2.gml", readerOpts);
    if (!model) {
        std::cerr << "加载失败\n";
        return 1;
    }

    // 打印摘要
    citygml::printSummary(*model);

    // 配置 OBJ 输出
    citygml::OBJOptions objOpts;
    objOpts.flipYZ = true;           // OpenGL 坐标系
    objOpts.centerModel = true;       // 居中
    objOpts.writeMTL = true;          // 包含材质
    objOpts.writeNormals = true;     // 包含法线

    // 写入
    citygml::save(*model, "output/building.obj", objOpts);

    return 0;
}
```

### C++ API 速查表

| 函数 | 说明 |
|------|------|
| `citygml::convert(input, output)` | 一行命令转换 |
| `citygml::load(path, options)` | 加载 CityGML，返回 CityModel 指针 |
| `citygml::save(model, path, options)` | 保存为 OBJ |
| `citygml::getBoundingBox(model)` | 获取边界框 |
| `citygml::listObjectTypes(model)` | 列出所有对象类型 |
| `citygml::countObjects(model)` | 统计每类对象数量 |
| `citygml::printSummary(model)` | 打印完整摘要 |

## 测试数据

项目 `testdata/` 目录已包含一套 CityGML 2.0 示例数据：

| 文件 | 说明 |
|------|------|
| `testdata/2.gml` | CityGML 2.0 建筑数据，23 栋建筑物，LOD2 几何，包含外观纹理引用 |
| `testdata/2_appearance/*.png` | 23 张纹理图片 |
| `testdata/2.gfs` | GDAL 配置文件 |

数据详情：
- **格式**：CityGML 2.0
- **坐标系统**：EPSG:4547（中国大地坐标系）
- **对象类型**：Building（LOD2 MultiSurface）
- **外观**：X3DMaterial（纯色）+ ParameterizedTexture（带纹理）
- **包围盒**：(6690.093, 2381.923, 3.803) ~ (6713.727, 2412.968, 10.736)

### 其他推荐数据源

如需更多测试数据，参见以下公开资源：

| 数据集 | 下载地址 | 说明 |
|--------|----------|------|
| **KIT CityGML 示例** | citygmlwiki.org | 学习入门 |
| **3D BAG（荷兰）** | 3dbag.nl | 大规模建筑数据 |
| **PLATEAU（日本）** | mlit.go.jp/plateau | 200+ 城市，官方开放 |
| **Random3Dcity** | github.com/tudelft3d/Random3Dcity | 程序化生成 |
| **柏林数据** | businesslocationcenter.de | LOD2 含纹理 |

## 示例输出

使用 `testdata/2.gml` 转换后的输出示例：

```bash
$ ./citygml2obj testdata/2.gml --blender --stats
加载: testdata/2.gml
========================================
  CityGML Model Summary
========================================
  ID:
  BoundingBox:
    Min: (6690.093, 2381.923, 3.803)
    Max: (6713.727, 2412.968, 10.736)
    Size: 23.634 x 31.045 x 6.933

  Object counts by type:
    Building                      : 23
    --------------------------------
    TOTAL                         : 23
========================================
写入: testdata/2.obj
材质: testdata/2.mtl
转换成功！
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
| TinyXML-2 | 8.1.0 | XML 解析 | Zlib | CMake FetchContent |
| Earcut | 2.2.4 | 多边形三角化 | ISC | CMake FetchContent |
| CMake | ≥3.16 | 构建系统 | BSD-3 | 系统工具 |

## 许可证

项目代码遵循 MIT 许可证。第三方依赖各自遵循其许可证（Zlib / ISC）。

## 已知限制

- 仅支持 CityGML 的几何数据，暂不支持拓扑关系（Topology）
- 隐式几何（ImplicitGeometry）的模板库引用需要外部提供模板文件
- 坐标参考系（CRS）转换需要额外的 proj 库支持（当前版本保留原始坐标）
- CityGML 3.0 支持待完善（Schema 层面支持，API 兼容）
