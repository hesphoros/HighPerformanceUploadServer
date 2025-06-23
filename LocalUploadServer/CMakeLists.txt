cmake_minimum_required(VERSION 3.15)
project(LocalUploadServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg/Qt路径
set(CMAKE_PREFIX_PATH "D:/cppsoft/vcpkg/installed/x64-windows")
set(CMAKE_TOOLCHAIN_FILE "D:/cppsoft/vcpkg/scripts/buildsystems/vcpkg.cmake")

# 查找 FlatBuffers
find_package(Flatbuffers CONFIG REQUIRED)

# 包含3rdParty头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/3rdParty/include
    ${CMAKE_SOURCE_DIR}/3rdParty/include/asio
    ${CMAKE_SOURCE_DIR}/3rdParty/include/hash-library
    ${CMAKE_SOURCE_DIR}/3rdParty/include/log
    ${CMAKE_SOURCE_DIR}/3rdParty/include/nlohmann
    ${CMAKE_SOURCE_DIR}/include
    $<TARGET_PROPERTY:flatbuffers::flatbuffers,INTERFACE_INCLUDE_DIRECTORIES>
)

# 包含FlatBuffers头文件
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer
)


# ---------------- FlatBuffers 自动生成配置 ----------------
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

# 收集 FlatBuffer 目录下所有 _generated.h/_generated.cpp 文件
file(GLOB FLATBUFFERS_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/*_generated.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/*_generated.cpp"
)

# ---------------------------------------------------------

# 查找源文件
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/AsioLoopbackIpcServer/*.cpp
    ${CMAKE_SOURCE_DIR}/3rdParty/src/hash-library/*.cpp
    ${CMAKE_SOURCE_DIR}/3rdParty/src/log/*.cpp
    # ${CMAKE_SOURCE_DIR}/3rdParty/src/asio/*.cpp
)

# 添加可执行文件，并包含 FlatBuffers 生成的源文件
add_executable(LocalUploadServer
    ${SRC_FILES}
    ${FLATBUFFERS_GENERATED_SOURCES}
)

add_dependencies(LocalUploadServer FlatBuffersGen)

# 链接静态库（如有）
target_link_libraries(LocalUploadServer
    ${CMAKE_SOURCE_DIR}/3rdParty/lib/iconv/libiconv_1_17.lib
    flatbuffers::flatbuffers
)

target_include_directories(LocalUploadServer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rdParty/include
    ${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer
)