# ===================== 基础配置 =====================
cmake_minimum_required(VERSION 3.15)
project(LocalUploadServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 强制源文件编码为UTF-8（适用于MSVC/VS）
if(MSVC)
    add_compile_options("/utf-8")
endif()

# vcpkg/Qt路径  
set(CMAKE_PREFIX_PATH "D:/cppsoft/vcpkg/installed/x64-windows")
set(CMAKE_TOOLCHAIN_FILE "D:/cppsoft/vcpkg/scripts/buildsystems/vcpkg.cmake")

# ===================== 依赖查找 =====================
find_package(Flatbuffers CONFIG REQUIRED)

# ===================== 头文件包含路径 =====================
include_directories(
    ${CMAKE_SOURCE_DIR}/3rdParty/include
    ${CMAKE_SOURCE_DIR}/3rdParty/include/asio
    ${CMAKE_SOURCE_DIR}/3rdParty/include/hash-library
    ${CMAKE_SOURCE_DIR}/3rdParty/include/log
    ${CMAKE_SOURCE_DIR}/3rdParty/include/nlohmann
    ${CMAKE_SOURCE_DIR}/3rdParty/include/tabulate
    ${CMAKE_SOURCE_DIR}/include
    $<TARGET_PROPERTY:flatbuffers::flatbuffers,INTERFACE_INCLUDE_DIRECTORIES>
    ${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer
)

# ===================== FlatBuffers 自动生成配置 =====================
file(GLOB FBS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/*.fbs")
set(FLATC_EXE "D:/cppsoft/vcpkg/installed/x64-windows/tools/flatbuffers/flatc.exe")

set(GEN_HEADERS)
set(GEN_SOURCES)
foreach(FBS_FILE ${FBS_FILES})
    get_filename_component(FBS_NAME ${FBS_FILE} NAME_WE)
    list(APPEND GEN_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/${FBS_NAME}_generated.h")
    list(APPEND GEN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/${FBS_NAME}_generated.cpp")
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/${FBS_NAME}_generated.h"
               "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/${FBS_NAME}_generated.cpp"
        COMMAND ${FLATC_EXE} --cpp --gen-object-api -o ${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer ${FBS_FILE}
        DEPENDS ${FBS_FILE}
        COMMENT "Generating FlatBuffers C++ code from ${FBS_FILE}"
    )
endforeach()
add_custom_target(FlatBuffersGen ALL DEPENDS ${GEN_HEADERS} ${GEN_SOURCES})

file(GLOB FLATBUFFERS_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/*_generated.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/*_generated.cpp"
)

# ===================== 源文件与头文件收集 =====================
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/AsioLoopbackIpcServer/*.cpp
    ${CMAKE_SOURCE_DIR}/3rdParty/src/hash-library/*.cpp
    ${CMAKE_SOURCE_DIR}/3rdParty/src/log/*.cpp
)

file(GLOB_RECURSE PROJECT_HEADERS
    ${CMAKE_SOURCE_DIR}/include/*.h
    ${CMAKE_SOURCE_DIR}/include/*.hpp
)

# ===================== 可执行文件与依赖 =====================
add_executable(LocalUploadServer
    ${SRC_FILES}
    ${FLATBUFFERS_GENERATED_SOURCES}
    ${PROJECT_HEADERS}
)

add_dependencies(LocalUploadServer FlatBuffersGen)

# ===================== 链接库 =====================
target_link_libraries(LocalUploadServer
    $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/lib/iconv/debug/libiconv_1_17.lib>
    $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/lib/iconv/release/libiconv_1_17.lib>
    flatbuffers::flatbuffers
)

# ===================== 目标包含路径 =====================
target_include_directories(LocalUploadServer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rdParty/include
    ${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer
    ${CMAKE_SOURCE_DIR}/3rdParty/include/tabulate
)

