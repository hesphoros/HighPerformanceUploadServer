cmake_minimum_required(VERSION 3.16)
project(UploadClient VERSION 1.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg/Qt路径
set(CMAKE_PREFIX_PATH "D:/cppsoft/vcpkg/installed/x64-windows")
#set(CMAKE_PREFIX_PATH "D:/Qt/6.5.3/msvc2019_64/lib/cmake")
# 查找Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(Protobuf REQUIRED)

# 启用Qt6的MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 源文件分组
set(SRC_MAIN src/main.cpp)
set(SRC_UI src/MainWindow.cpp src/FileListWidget.cpp)
set(SRC_UPLOAD src/SyncUploadQueue/Lusp_SyncUploadQueue.cpp src/SyncUploadQueue/Lusp_SyncUploadQueuePrivate.cpp src/NotificationService/Lusp_SyncFilesNotificationService.cpp)
set(SRC_FILEINFO src/FileInfo/FileInfo.cpp)
set(SRC_LOG src/log_headers.cpp 3rdParty/src/log/UniConv.cpp 3rdParty/src/log/LightLogWriteImpl.cpp)
set(SRC_HASH 3rdParty/src/hash-library/md5.cpp 3rdParty/src/hash-library/sha1.cpp 3rdParty/src/hash-library/sha256.cpp 3rdParty/src/hash-library/sha3.cpp 3rdParty/src/hash-library/crc32.cpp)

# 头文件分组
set(INC_UI include/MainWindow.h include/FileListWidget.h)
set(INC_UPLOAD include/SyncUploadQueue/Lusp_SyncUploadQueue.h src/SyncUploadQueue/Lusp_SyncUploadQueuePrivate.h include/ThreadSafeRowLockQueue/ThreadSafeRowLockQueue.hpp)
set(INC_FILEINFO include/FileInfo/FileInfo.h)
set(INC_HASH 3rdParty/include/hash-library/md5.h)

# UI文件
set(UI_FILES ui/MainWindow.ui)

# 合并所有源/头文件
set(ALL_SOURCES ${SRC_MAIN} ${SRC_UI} ${SRC_UPLOAD} ${SRC_FILEINFO} ${SRC_LOG} ${SRC_HASH})
set(ALL_HEADERS ${INC_UI} ${INC_UPLOAD} ${INC_FILEINFO} ${INC_HASH})

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

# 创建可执行文件
add_executable(UploadClient ${ALL_SOURCES} ${ALL_HEADERS} ${UI_FILES})

# 目标include目录
target_include_directories(UploadClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FileInfo
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SyncUploadQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ThreadSafeRowLockQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SyncUploadQueue
)

# Protobuf
target_include_directories(UploadClient PRIVATE ${Protobuf_INCLUDE_DIRS})
target_link_libraries(UploadClient PRIVATE ${Protobuf_LIBRARIES})

# Qt6
target_link_libraries(UploadClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    rpcrt4
    $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/lib/iconv/libiconv_1_17d.lib>
    $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/lib/iconv/libiconv_1_17.lib>
    Kernel32
)

# 输出目录
set_target_properties(UploadClient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

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
