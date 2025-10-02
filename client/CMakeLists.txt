cmake_minimum_required(VERSION 3.16)
project(UploadClient VERSION 1.0.0)

# 🚀 构建优化设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用构建缓存以加速重新构建
set(CMAKE_CXX_COMPILER_LAUNCHER "" CACHE STRING "")
if(WIN32)
    # Windows 上优化编译器设置
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # 启用并行编译
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

# vcpkg/Qt路径
set(CMAKE_PREFIX_PATH "D:/cppsoft/vcpkg/installed/x64-windows")
set(CMAKE_TOOLCHAIN_FILE "D:/cppsoft/vcpkg/scripts/buildsystems/vcpkg.cmake")

# 查找Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(FlatBuffers REQUIRED)

# TOML库支持 (使用本地toml11)
set(TOML11_LOCAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include/toml11")
if(EXISTS "${TOML11_LOCAL_PATH}/toml.hpp")
    message(STATUS "✅ 找到本地toml11库: ${TOML11_LOCAL_PATH}")
    set(TOML11_LOCAL_AVAILABLE TRUE)
else()
    message(WARNING "⚠️ 本地toml11库未找到: ${TOML11_LOCAL_PATH}/toml.hpp")
    set(TOML11_LOCAL_AVAILABLE FALSE)
endif()

# spdlog库支持 (使用FetchContent)
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.12.0  # 使用稳定版本
    GIT_SHALLOW    TRUE     # 只下载最新commit，加速下载
)

# 配置spdlog选项
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "")
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "")
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "")
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "")  # 使用内置fmt
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "")  # 静态链接

FetchContent_MakeAvailable(spdlog)
message(STATUS "✅ spdlog库已通过FetchContent集成")

# UniConv依赖
FetchContent_Declare(
    UniConv
    GIT_REPOSITORY https://github.com/hesphoros/UniConv.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(UniConv)
message(STATUS "✅ UniConv库已通过FetchContent集成")

# magic_enum库支持 (使用本地头文件)
set(MAGIC_ENUM_LOCAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/magic_enum")
if(EXISTS "${MAGIC_ENUM_LOCAL_PATH}/magic_enum.hpp")
    message(STATUS "✅ 找到本地magic_enum库: ${MAGIC_ENUM_LOCAL_PATH}")
    set(MAGIC_ENUM_LOCAL_AVAILABLE TRUE)
else()
    message(WARNING "⚠️ 本地magic_enum库未找到: ${MAGIC_ENUM_LOCAL_PATH}/magic_enum.hpp")
    set(MAGIC_ENUM_LOCAL_AVAILABLE FALSE)
endif()

# 启用Qt6的MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# FlatBuffers 配置
set(FBS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/FlatBuffer/upload_file_info.fbs)
set(FBS_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)


#abseil 配置 - 🚀 智能检测优化
find_package(absl CONFIG)
if(absl_FOUND)
    message(STATUS "✅ 使用 vcpkg 预编译 Abseil 库 (快速)")
    set(USING_VCPKG_ABSEIL TRUE)
else()
    message(STATUS "🔄 vcpkg Abseil 未找到，使用源码构建")
    set(USING_VCPKG_ABSEIL FALSE)
    
    # 优化源码构建设置
    set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "")
    set(ABSL_BUILD_TESTING OFF CACHE BOOL "")
    set(ABSL_BUILD_MONOLITHIC_SHARED_LIBS OFF CACHE BOOL "")
    set(ABSL_USE_SYSTEM_INCLUDES ON CACHE BOOL "")
    
    if(NOT TARGET absl::base)
        message(STATUS "🔄 正在构建 Abseil 库... (这可能需要几分钟)")
        add_subdirectory(3rdParty/abseil-cpp)
        message(STATUS "✅ Abseil 源码构建完成")
    else()
        message(STATUS "⚡ Abseil 已缓存，跳过构建")
    endif()
endif()

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
set(SRC_LOG src/log_headers.cpp)
set(SRC_HASH 3rdParty/src/hash-library/md5.cpp 3rdParty/src/hash-library/sha1.cpp 3rdParty/src/hash-library/sha256.cpp 3rdParty/src/hash-library/sha3.cpp 3rdParty/src/hash-library/crc32.cpp)
set(SRC_LOOPBACK src/AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.cpp)
set(SRC_CONFIG src/Config/ClientConfigManager.cpp)
set(SRC_MSGQUEUE src/MessageQueue/PersistentMessageQueue.cpp src/MessageQueue/ConnectionMonitor.cpp)
# 头文件分组
set(INC_UI include/MainWindow.h include/FileListWidget.h)
set(INC_UPLOAD include/SyncUploadQueue/Lusp_SyncUploadQueue.h src/SyncUploadQueue/Lusp_SyncUploadQueuePrivate.h include/ThreadSafeRowLockQueue/ThreadSafeRowLockQueue.hpp)
set(INC_FILEINFO include/FileInfo/FileInfo.h)
set(INC_HASH 3rdParty/include/hash-library/md5.h)
set(INC_LOOPBACK include/AsioLoopbackIpcClient/Lusp_AsioLoopbackIpcClient.h)
set(INC_CONFIG include/Config/ClientConfigManager.h)
# UI文件
set(UI_FILES ui/MainWindow.ui)

# 合并所有源/头文件（FlatBuffers 只需加 FLATBUFFERS_GENERATED_SOURCES）
set(ALL_SOURCES ${SRC_MAIN} ${SRC_UI} ${SRC_UPLOAD} ${SRC_FILEINFO} ${SRC_LOG} ${SRC_HASH} ${SRC_LOOPBACK} ${SRC_CONFIG} ${SRC_MSGQUEUE} ${FLATBUFFERS_GENERATED_SOURCES})
set(ALL_HEADERS ${INC_UI} ${INC_UPLOAD} ${INC_FILEINFO} ${INC_HASH} ${INC_LOOPBACK} ${INC_CONFIG})

# include目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FileInfo
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SyncUploadQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ThreadSafeRowLockQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Config
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include/hash-library
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include/log
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SyncUploadQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/uniconv-src/include
)

# 添加本地toml11头文件路径
if(TOML11_LOCAL_AVAILABLE)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include)
endif()

# 添加本地magic_enum头文件路径
if(MAGIC_ENUM_LOCAL_AVAILABLE)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/magic_enum)
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/uniconv-src/include
)

# FlatBuffers
# 添加到可执行文件
# FlatBuffers 头文件已包含在 INC_FBS

# 链接库
# FlatBuffers
# Qt6
# abseil
# 其他依赖

target_link_libraries(UploadClient PRIVATE flatbuffers::flatbuffers)

# 基础库链接
target_link_libraries(UploadClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    rpcrt4
    Kernel32
)

# Abseil库链接 (本地源码构建)
target_link_libraries(UploadClient PRIVATE
    absl::log_severity
    absl::strings
    absl::base
    absl::log
    absl::time
)
if(USING_VCPKG_ABSEIL)
    message(STATUS "✅ 链接 vcpkg Abseil 预编译库 (快速)")
else()
    message(STATUS "⚠️ 链接本地构建 Abseil 库 (较慢)")
    message(STATUS "💡 提示: 运行 'vcpkg install abseil:x64-windows' 可显著加速后续构建")
endif()

# spdlog库链接
target_link_libraries(UploadClient PRIVATE spdlog::spdlog)
message(STATUS "✅ 使用spdlog库 (FetchContent)")

# UniConv库链接
target_link_libraries(UploadClient PRIVATE UniConv::UniConv)
message(STATUS "✅ 使用UniConv库 (FetchContent)")

# 配置TOML库
if(TOML11_LOCAL_AVAILABLE)
    target_include_directories(UploadClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include)
    target_compile_definitions(UploadClient PRIVATE TOML11_AVAILABLE)
    message(STATUS "✅ 使用本地toml11库: ${TOML11_LOCAL_PATH}")
else()
    message(WARNING "⚠️ toml11 not found. TOML configuration features will be limited.")
    target_compile_definitions(UploadClient PRIVATE NO_TOML_SUPPORT)
endif()

# 配置magic_enum库
if(MAGIC_ENUM_LOCAL_AVAILABLE)
    target_include_directories(UploadClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/magic_enum)
    target_compile_definitions(UploadClient PRIVATE MAGIC_ENUM_AVAILABLE)
    message(STATUS "✅ 使用本地magic_enum库: ${MAGIC_ENUM_LOCAL_PATH}")
else()
    message(WARNING "⚠️ magic_enum not found. Enum reflection features will be limited.")
    target_compile_definitions(UploadClient PRIVATE NO_MAGIC_ENUM_SUPPORT)
endif()

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
