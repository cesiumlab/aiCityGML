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

# Debug: 关闭默认的 /RTC1（与 /O2 不兼容），使用 /Od
set(CMAKE_CXX_FLAGS_DEBUG "/MDd /W4 /Od /Ob0 /Zi /D_DEBUG /EHsc")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /RTC1-")

# Release: 优化选项
set(CMAKE_CXX_FLAGS_RELEASE "/MD /W4 /O2 /Ob2 /GL /D NDEBUG /EHsc")

# ============================================================
# 依赖库（CMake file(DOWNLOAD) 方式获取）
# ============================================================
# tinyxml2: https://github.com/leethomason/tinyxml2
#   版本: >= 9.0.0（推荐 9.0.0）
# earcut:   https://github.com/mapbox/earcut.hpp
#   版本: 任意（推荐 v2.2.4）
# ============================================================
set(_DEPS_DIR "${CMAKE_BINARY_DIR}/_deps")
file(MAKE_DIRECTORY "${_DEPS_DIR}")

# --- tinyxml2 ---
set(_TINYXML2_ZIP  "${_DEPS_DIR}/tinyxml2-9.0.0.zip")
if(NOT EXISTS "${_TINYXML2_ZIP}")
    message(STATUS "Downloading tinyxml2 9.0.0...")
    file(DOWNLOAD https://github.com/leethomason/tinyxml2/archive/refs/tags/9.0.0.zip
        "${_TINYXML2_ZIP}" TIMEOUT 120 STATUS _DL)
    list(GET _DL 0 _CODE)
    if(NOT _CODE EQUAL 0)
        message(FATAL_ERROR "tinyxml2 download failed: ${_DL}")
    endif()
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xzf "${_TINYXML2_ZIP}"
    WORKING_DIRECTORY "${_DEPS_DIR}" RESULT_VARIABLE _RET)
if(NOT _RET EQUAL 0)
    message(FATAL_ERROR "tinyxml2 extract failed")
endif()
# zip 带顶层目录，解压后为 _deps/tinyxml2-9.0.0/
file(GLOB _TX2 "${_DEPS_DIR}/tinyxml2-9.0.0")
set(TINYXML2_ROOT "${_TX2}")
add_subdirectory("${TINYXML2_ROOT}" "${_DEPS_DIR}/tinyxml2-build" EXCLUDE_FROM_ALL)

# --- earcut ---
set(_EARCUT_ZIP  "${_DEPS_DIR}/earcut-v2.2.4.zip")
if(NOT EXISTS "${_EARCUT_ZIP}")
    message(STATUS "Downloading earcut v2.2.4...")
    file(DOWNLOAD https://github.com/mapbox/earcut.hpp/archive/refs/tags/v2.2.4.zip
        "${_EARCUT_ZIP}" TIMEOUT 120 STATUS _DL)
    list(GET _DL 0 _CODE)
    if(NOT _CODE EQUAL 0)
        message(FATAL_ERROR "earcut download failed: ${_DL}")
    endif()
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xzf "${_EARCUT_ZIP}"
    WORKING_DIRECTORY "${_DEPS_DIR}" RESULT_VARIABLE _RET)
if(NOT _RET EQUAL 0)
    message(FATAL_ERROR "earcut extract failed")
endif()
# zip 带顶层目录，解压后为 _deps/earcut.hpp-v2.2.4/
file(GLOB _EC "${_DEPS_DIR}/earcut*")
list(FILTER _EC EXCLUDE REGEX "\\.zip$")  # 排除 zip 文件
set(EARCUT_ROOT "${_EC}")

# ============================================================
# 头文件目录
# ============================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${TINYXML2_ROOT}
    ${EARCUT_ROOT}/include
)

# ============================================================
# CityGML Core 模块
# ============================================================
set(CITYGML_CORE_HEADERS
    src/citygml/core/citygml_base.h
    src/citygml/core/citygml_feature.h
    src/citygml/core/citygml_geometry.h
    src/citygml/core/citygml_space.h
    src/citygml/core/citygml_surface.h
    src/citygml/core/citygml_appearance.h
    src/citygml/core/citygml_object.h
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
    src/citygml/building/citygml_building.h
)

set(CITYGML_BUILDING_SOURCES
    src/building/citygml_building.cpp
)

# ============================================================
# CityGML Construction 模块
# ============================================================
set(CITYGML_CONSTRUCTION_HEADERS
    src/citygml/construction/citygml_construction.h
)

set(CITYGML_CONSTRUCTION_SOURCES
    src/construction/citygml_construction.cpp
)

# ============================================================
# CityGML Parser 模块
# ============================================================
set(CITYGML_PARSER_HEADERS
    src/citygml/citygml_parser.h
    src/citygml/parser/parser_context.h
    src/citygml/parser/xml_document.h
    src/citygml/parser/gml_envelope_parser.h
    src/citygml/parser/gml_geometry_parser.h
    src/citygml/parser/citygml_reader.h
    src/citygml/parser/xml_utils.h
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
    src/citygml/mesh/mesh.h
    src/citygml/mesh/mesh_generator.h
    src/citygml/writers/obj_writer.h
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
target_link_libraries(aicitygml PRIVATE tinyxml2)
target_include_directories(aicitygml PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${TINYXML2_ROOT}>
    $<BUILD_INTERFACE:${EARCUT_ROOT}/include>
    $<INSTALL_INTERFACE:include>
)

# ============================================================
# 测试程序
# ============================================================
add_executable(test_parser src/test/test_parser.cpp)
target_link_libraries(test_parser PRIVATE aicitygml)

add_executable(test_triangulate src/test/test_triangulate.cpp)

# ============================================================
# 安装规则
# ============================================================
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # CMake 默认安装到 C:/Program Files/...，改为项目下的 install/ 目录
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Installation prefix" FORCE)
endif()

# --- 1. citygml 公共头文件 ---
install(DIRECTORY src/citygml
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")

# --- 2. tinyxml2 头文件 ---
install(FILES ${TINYXML2_ROOT}/tinyxml2.h
        DESTINATION include/tinyxml2)

# --- 3. earcut 头文件 ---
install(FILES ${EARCUT_ROOT}/include/mapbox/earcut.hpp
        DESTINATION include/mapbox)

# --- 4. 静态库 (.lib) ---
install(TARGETS aicitygml tinyxml2
        EXPORT aicitygml-targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)

# --- 5. 导出 CMake 配置（供外部项目 find_package 使用）---
install(EXPORT aicitygml-targets
        FILE aicitygml-targets.cmake
        NAMESPACE aicitygml::
        DESTINATION lib/cmake/aicitygml)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/aicitygml-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/aicitygml-config.cmake
    INSTALL_DESTINATION lib/cmake/aicitygml)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/aicitygml-config.cmake
        DESTINATION lib/cmake/aicitygml)
