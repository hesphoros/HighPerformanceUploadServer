cmake_minimum_required(VERSION 3.16)
project(UploadClient VERSION 1.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg/Qt路径
set(CMAKE_PREFIX_PATH "D:/cppsoft/vcpkg/installed/x64-windows")
set(CMAKE_TOOLCHAIN_FILE "D:/cppsoft/vcpkg/scripts/buildsystems/vcpkg.cmake")

# 查找Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(FlatBuffers REQUIRED)
find_package(absl REQUIRED)

# 启用Qt6的MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# FlatBuffers 配置
set(FBS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/upload_file_info.fbs)
set(FBS_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)


# 自动查找 FlatBuffer 目录下所有 .fbs 文件并生成对应 .h/.cc
file(GLOB FBS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/*.fbs")

# 指定 flatc.exe 的绝对路径
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

# 收集 FlatBuffer 目录下所有 _generated.h/_generated.cpp 文件（只收 flatc 生成的文件）
file(GLOB FLATBUFFERS_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/*_generated.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/*_generated.cpp"
)

set(SRC_FBS ${FLATBUFFERS_GENERATED_SOURCES})

# 源文件分组
set(SRC_MAIN src/main.cpp)
set(SRC_UI src/MainWindow.cpp src/FileListWidget.cpp)
set(SRC_UPLOAD src/SyncUploadQueue/Lusp_SyncUploadQueue.cpp src/SyncUploadQueue/Lusp_SyncUploadQueuePrivate.cpp src/NotificationService/Lusp_SyncFilesNotificationService.cpp)
set(SRC_FILEINFO src/FileInfo/FileInfo.cpp)
set(SRC_LOG src/log_headers.cpp 3rdParty/src/log/UniConv.cpp 3rdParty/src/log/LightLogWriteImpl.cpp)
set(SRC_HASH 3rdParty/src/hash-library/md5.cpp 3rdParty/src/hash-library/sha1.cpp 3rdParty/src/hash-library/sha256.cpp 3rdParty/src/hash-library/sha3.cpp 3rdParty/src/hash-library/crc32.cpp)
set(SRC_LOOPBACK src/AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.cpp)
# 头文件分组
set(INC_UI include/MainWindow.h include/FileListWidget.h)
set(INC_UPLOAD include/SyncUploadQueue/Lusp_SyncUploadQueue.h src/SyncUploadQueue/Lusp_SyncUploadQueuePrivate.h include/ThreadSafeRowLockQueue/ThreadSafeRowLockQueue.hpp)
set(INC_FILEINFO include/FileInfo/FileInfo.h)
set(INC_HASH 3rdParty/include/hash-library/md5.h)
set(INC_LOOPBACK include/AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h)
# UI文件
set(UI_FILES ui/MainWindow.ui)

# 合并所有源/头文件（FlatBuffers 只需加 FLATBUFFERS_GENERATED_SOURCES）
set(ALL_SOURCES ${SRC_MAIN} ${SRC_UI} ${SRC_UPLOAD} ${SRC_FILEINFO} ${SRC_LOG} ${SRC_HASH} ${SRC_LOOPBACK} ${FLATBUFFERS_GENERATED_SOURCES})
set(ALL_HEADERS ${INC_UI} ${INC_UPLOAD} ${INC_FILEINFO} ${INC_HASH} ${INC_LOOPBACK})

# include目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FileInfo
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SyncUploadQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ThreadSafeRowLockQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include/hash-library
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include/log
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SyncUploadQueue
)

# 包含生成目录
include_directories(${FBS_GEN_DIR})

# 添加 FlatBuffer 目录到 include 路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer)

# 创建可执行文件
add_executable(UploadClient ${ALL_SOURCES} ${ALL_HEADERS} ${UI_FILES})
add_dependencies(UploadClient FlatBuffersGen)

# 目标include目录
target_include_directories(UploadClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FileInfo
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SyncUploadQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ThreadSafeRowLockQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SyncUploadQueue
)

# FlatBuffers
# 添加到可执行文件
# FlatBuffers 头文件已包含在 INC_FBS

# 链接库
# FlatBuffers
# Qt6
# 其他依赖

target_link_libraries(UploadClient PRIVATE flatbuffers::flatbuffers)

target_link_libraries(UploadClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    rpcrt4
    $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/lib/iconv/debug/libiconv_1_17.lib>
    $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/lib/iconv/release/libiconv_1_17.lib>
    Kernel32
    absl::log_severity
    absl::strings
    absl::base
    absl::log
)

# 输出目录
set_target_properties(UploadClient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(UploadClient PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")

# Windows特定设置
if(WIN32)
    set_target_properties(UploadClient PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# 编译器特定设置
if(MSVC)
    target_compile_options(UploadClient PRIVATE /W3)
    target_compile_definitions(UploadClient PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
    # MSVC: 强制 __cplusplus 宏正确
    add_compile_options(/Zc:__cplusplus)
    add_definitions(-D_WIN32_WINNT=0x0601)
else()
    target_compile_options(UploadClient PRIVATE -Wall -Wextra)
endif()

# 调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(UploadClient PRIVATE DEBUG)
endif()

# 输出构建信息
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Qt6 Version: ${Qt6_VERSION}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Sources: ${ALL_SOURCES}")
message(STATUS "Headers: ${ALL_HEADERS}")
