cmake_minimum_required(VERSION 3.15)
project(LocalUploadServer)

# 指定工具链文件（如有交叉编译需求）

# C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg/Qt路径
set(CMAKE_PREFIX_PATH "D:/cppsoft/vcpkg/installed/x64-windows")

if(NOT Protobuf_PROTOC_EXECUTABLE)
    set(Protobuf_PROTOC_EXECUTABLE "D:/cppsoft/vcpkg/installed/x64-windows/tools/protobuf/protoc.exe")
endif()


# 包含3rdParty头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/3rdParty/include
    ${CMAKE_SOURCE_DIR}/3rdParty/include/asio
    ${CMAKE_SOURCE_DIR}/3rdParty/include/hash-library
    ${CMAKE_SOURCE_DIR}/3rdParty/include/log
    ${CMAKE_SOURCE_DIR}/3rdParty/include/nlohmann
    ${CMAKE_SOURCE_DIR}/include
)

# 查找源文件
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/AsioLoopbackIpcServer/*.cpp
    ${CMAKE_SOURCE_DIR}/3rdParty/src/hash-library/*.cpp
    ${CMAKE_SOURCE_DIR}/3rdParty/src/log/*.cpp
    # ${CMAKE_SOURCE_DIR}/3rdParty/src/asio/*.cpp
)

# 添加可执行文件
add_executable(LocalUploadServer ${SRC_FILES})

# 链接静态库（如有）
target_link_libraries(LocalUploadServer
    ${CMAKE_SOURCE_DIR}/3rdParty/lib/iconv/libiconv_1_17.lib
)

# 头文件目录
target_include_directories(LocalUploadServer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rdParty/include
)
