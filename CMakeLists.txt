cmake_minimum_required(VERSION 3.16)
project(citygml-to-obj VERSION 1.0.0 LANGUAGES CXX)

# ============================================================
# 编译选项
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Debug: 关闭默认的 /RTC1（与 /O2 不兼容），使用 /Od
set(CMAKE_CXX_FLAGS_DEBUG "/MDd /W4 /Od /Ob0 /Zi /D_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /RTC1-")

# Release: 优化选项
set(CMAKE_CXX_FLAGS_RELEASE "/MD /W4 /O2 /Ob2 /GL /D NDEBUG")

# ============================================================
# 依赖库
# ============================================================
set(TINYXML2_ROOT "${CMAKE_SOURCE_DIR}/tinyxml2-master")
set(TINYXML2_LIB  "${CMAKE_SOURCE_DIR}/tinyxml2-master/build/tinyxml2.lib")
set(EARCUT_ROOT   "${CMAKE_SOURCE_DIR}/earcut.hpp-master")

if(NOT EXISTS "${TINYXML2_ROOT}/tinyxml2.h")
    message(FATAL_ERROR "请先编译 TinyXML-2: e:\\opensource\\citygml\\tinyxml2-master")
endif()
if(NOT EXISTS "${EARCUT_ROOT}/include/mapbox/earcut.hpp")
    message(FATAL_ERROR "请先编译 Earcut: e:\\opensource\\citygml\\earcut.hpp-master")
endif()

# ============================================================
# 头文件目录
# ============================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TINYXML2_ROOT}
    ${EARCUT_ROOT}/include
)

# ============================================================
# CityGML Core 模块
# ============================================================
set(CITYGML_CORE_HEADERS
    include/citygml/core/citygml_base.h
    include/citygml/core/citygml_feature.h
    include/citygml/core/citygml_geometry.h
    include/citygml/core/citygml_space.h
    include/citygml/core/citygml_surface.h
    include/citygml/core/citygml_appearance.h
    include/citygml/core/citygml_object.h
)

set(CITYGML_CORE_SOURCES
    src/core/citygml_base.cpp
    src/core/citygml_feature.cpp
    src/core/citygml_geometry.cpp
    src/core/citygml_space.cpp
    src/core/citygml_surface.cpp
    src/core/citygml_appearance.cpp
    src/core/citygml_object.cpp
)

# ============================================================
# CityGML Building 模块
# ============================================================
set(CITYGML_BUILDING_HEADERS
    include/citygml/building/citygml_building.h
)

set(CITYGML_BUILDING_SOURCES
    src/building/citygml_building.cpp
)

# ============================================================
# CityGML Construction 模块
# ============================================================
set(CITYGML_CONSTRUCTION_HEADERS
    include/citygml/construction/citygml_construction.h
)

set(CITYGML_CONSTRUCTION_SOURCES
    src/construction/citygml_construction.cpp
)

# ============================================================
# CityGML Parser 模块
# ============================================================
set(CITYGML_PARSER_HEADERS
    include/citygml/citygml_parser.h
    include/citygml/parser/parser_context.h
    include/citygml/parser/xml_document.h
    include/citygml/parser/gml_envelope_parser.h
    include/citygml/parser/gml_geometry_parser.h
    include/citygml/parser/citygml_reader.h
    include/citygml/parser/xml_utils.h
)

set(CITYGML_PARSER_SOURCES
    src/parser/citygml_parser.cpp
    src/parser/parser_context.cpp
    src/parser/xml_document.cpp
    src/parser/gml_envelope_parser.cpp
    src/parser/gml_geometry_parser.cpp
    src/parser/citygml_reader.cpp
    src/parser/xml_utils.cpp
)

# ============================================================
# 合并所有源文件
# ============================================================
set(ALL_HEADERS
    ${CITYGML_CORE_HEADERS}
    ${CITYGML_BUILDING_HEADERS}
    ${CITYGML_CONSTRUCTION_HEADERS}
    ${CITYGML_PARSER_HEADERS}
)

set(ALL_SOURCES
    ${CITYGML_CORE_SOURCES}
    ${CITYGML_BUILDING_SOURCES}
    ${CITYGML_CONSTRUCTION_SOURCES}
    ${CITYGML_PARSER_SOURCES}
)

add_library(citygml_obj_lib STATIC ${ALL_HEADERS} ${ALL_SOURCES})
target_link_libraries(citygml_obj_lib PRIVATE "${TINYXML2_LIB}")

# ============================================================
# 安装规则
# ============================================================
install(DIRECTORY include/citygml
        DESTINATION include)