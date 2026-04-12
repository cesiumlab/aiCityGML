cmake_minimum_required(VERSION 3.16)
project(aiCityGML VERSION 1.0.0 LANGUAGES CXX)

# ============================================================
# 编译选项
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Debug 库后缀 d
set(CMAKE_DEBUG_POSTFIX "d")

# VS2019 兼容性设置
if(MSVC_VERSION LESS 1930)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
endif()

# MSVC: Force UTF-8 source encoding
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_compile_definitions($<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS)

# VS2019 /std:c++17
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")

# Debug: 关闭默认的 /RTC1（与 /O2 不兼容），使用 /Od
set(CMAKE_CXX_FLAGS_DEBUG "/MDd /W4 /Od /Ob0 /Zi /D_DEBUG /EHsc /RTC1-")

# Release: 优化选项
set(CMAKE_CXX_FLAGS_RELEASE "/MD /W4 /O2 /Ob2 /GL /D NDEBUG /EHsc")

# ============================================================
# 依赖库
# ============================================================
# --- tinyxml2: 手动安装版 E:/opensource/install/tinyxml2 ---
set(TINYXML2_ROOT "E:/opensource/install/tinyxml2")
find_path(TINYXML2_INCLUDE_DIR tinyxml2.h PATHS "${TINYXML2_ROOT}/include")
find_library(TINYXML2_LIBRARY tinyxml2 PATHS "${TINYXML2_ROOT}/lib" NO_DEFAULT_PATH)
if(NOT TINYXML2_LIBRARY)
    find_library(TINYXML2_LIBRARY tinyxml2d PATHS "${TINYXML2_ROOT}/lib/debug" NO_DEFAULT_PATH)
endif()
if(TINYXML2_LIBRARY)
    message(STATUS "Found tinyxml2: ${TINYXML2_LIBRARY}")
else()
    message(FATAL_ERROR "tinyxml2 library not found in ${TINYXML2_ROOT}/lib")
endif()

# --- earcut: 本地 E:/opensource/earcut/earcut.hpp/include/ ---
set(EARCUT_ROOT "E:/opensource/earcut/earcut.hpp/include")

# ============================================================
# 头文件目录
# ============================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${TINYXML2_INCLUDE_DIR}
    ${EARCUT_ROOT}
)

# ============================================================
# CityGML Core 模块
# ============================================================
set(CITYGML_CORE_HEADERS
    src/core/citygml_base.h
    src/core/citygml_feature.h
    src/core/citygml_geometry.h
    src/core/citygml_space.h
    src/core/citygml_surface.h
    src/core/citygml_appearance.h
    src/core/citygml_object.h
    src/core/citygml_opening.h
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
    src/building/citygml_building.h
)

set(CITYGML_BUILDING_SOURCES
    src/building/citygml_building.cpp
)

# ============================================================
# CityGML Construction 模块
# ============================================================
set(CITYGML_CONSTRUCTION_HEADERS
    src/construction/citygml_construction.h
)

set(CITYGML_CONSTRUCTION_SOURCES
    src/construction/citygml_construction.cpp
)

# ============================================================
# CityGML Parser 模块
# ============================================================
set(CITYGML_PARSER_HEADERS
    src/parser/citygml_parser.h
    src/parser/parser_context.h
    src/parser/xml_document.h
    src/parser/gml_envelope_parser.h
    src/parser/gml_geometry_parser.h
    src/parser/citygml_reader.h
    src/parser/xml_utils.h
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
# CityGML Mesh 模块
# ============================================================
set(CITYGML_MESH_HEADERS
    src/mesh/mesh.h
    src/mesh/mesh_generator.h
    src/writers/obj_writer.h
)

set(CITYGML_MESH_SOURCES
    src/mesh/mesh.cpp
    src/mesh/mesh_generator.cpp
    src/writers/obj_writer.cpp
)

# ============================================================
# 合并所有源文件
# ============================================================
set(ALL_HEADERS
    ${CITYGML_CORE_HEADERS}
    ${CITYGML_BUILDING_HEADERS}
    ${CITYGML_CONSTRUCTION_HEADERS}
    ${CITYGML_MESH_HEADERS}
    ${CITYGML_PARSER_HEADERS}
)

set(ALL_SOURCES
    ${CITYGML_CORE_SOURCES}
    ${CITYGML_BUILDING_SOURCES}
    ${CITYGML_CONSTRUCTION_SOURCES}
    ${CITYGML_MESH_SOURCES}
    ${CITYGML_PARSER_SOURCES}
)

add_library(aicitygml STATIC ${ALL_HEADERS} ${ALL_SOURCES})
target_link_libraries(aicitygml PUBLIC
    $<$<CONFIG:Debug>:${TINYXML2_ROOT}/lib/debug/tinyxml2d.lib>
    $<$<CONFIG:Release>:${TINYXML2_ROOT}/lib/release/tinyxml2.lib>
)
target_include_directories(aicitygml PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${TINYXML2_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${EARCUT_ROOT}>
    $<INSTALL_INTERFACE:include>
)

# ============================================================
# 测试程序
# ============================================================
add_executable(test_parser src/test/test_parser.cpp)
target_link_libraries(test_parser PRIVATE aicitygml)

add_executable(test_srsname src/test/test_srsname.cpp)
target_link_libraries(test_srsname PRIVATE aicitygml)

add_executable(test_triangulate src/test/test_triangulate.cpp)

# ============================================================
# 安装规则
# ============================================================
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Installation prefix" FORCE)
endif()

# --- 1. citygml 公共头文件 ---
install(DIRECTORY src/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")

# --- 2. tinyxml2 头文件 ---
install(FILES ${TINYXML2_INCLUDE_DIR}/tinyxml2.h
        DESTINATION include/tinyxml2)

# --- 3. earcut 头文件 ---
install(FILES ${EARCUT_ROOT}/mapbox/earcut.hpp
        DESTINATION include/mapbox)

# --- 4. tinyxml2 Debug/Release lib ---
install(FILES ${TINYXML2_ROOT}/lib/debug/tinyxml2d.lib
        DESTINATION lib
        RENAME tinyxml2d.lib)
install(FILES ${TINYXML2_ROOT}/lib/release/tinyxml2.lib
        DESTINATION lib)

# --- 5. 静态库 (.lib) ---
install(TARGETS aicitygml
        EXPORT aicitygml-targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)

# --- 6. 导出 CMake 配置（供外部项目 find_package 使用）---
install(EXPORT aicitygml-targets
        FILE aicitygml-targets.cmake
        NAMESPACE aicitygml::
        DESTINATION lib/cmake/aicitygml)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/aicitygml-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/aicitygml-config.cmake
    INSTALL_DESTINATION lib/cmake/aicitygml
    PATH_VARS TINYXML2_ROOT TINYXML2_INCLUDE_DIR)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/aicitygml-config.cmake
        DESTINATION lib/cmake/aicitygml)
