cmake_minimum_required(VERSION 3.16)

project(UploadClient VERSION 1.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Qt6路径
set(CMAKE_PREFIX_PATH "D:\\Qt\\6.5.3\\msvc2019_64")

# 查找Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# 启用Qt6的MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 源文件（按模块组织）
set(MAIN_SOURCES
    src/main.cpp
)

set(UI_SOURCES
    src/MainWindow.cpp
    src/FileListWidget.cpp
)

set(UPLOAD_SOURCES
    src/SyncUploadQueue/Lusp_SyncUploadQueue.cpp
)

set(FILEINFO_SOURCES
    src/FileInfoWrapper.cpp
)

# 头文件（按模块组织）
set(UI_HEADERS
    include/MainWindow.h
    include/FileListWidget.h
)

set(UPLOAD_HEADERS
    include/SyncUploadQueue/Lusp_SyncUploadQueue.h
    include/ThreadSafeRowLockQueue/ThreadSafeRowLockQueue.hpp
)

set(FILEINFO_HEADERS
    include/FileInfo/FileInfo.h
    include/FileInfoWrapper.h
)

set(THIRDPARTY_HEADERS
    3rdParty/include/md5.h
)

# 合并所有源文件和头文件
set(ALL_SOURCES
    ${MAIN_SOURCES}
    ${UI_SOURCES}
    ${UPLOAD_SOURCES}
    ${FILEINFO_SOURCES}
)

set(ALL_HEADERS
    ${UI_HEADERS}
    ${UPLOAD_HEADERS}
    ${FILEINFO_HEADERS}
    ${THIRDPARTY_HEADERS}
)

# UI文件
set(UI_FILES
    ui/MainWindow.ui
)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FileInfo
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SyncUploadQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ThreadSafeRowLockQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include
)

# 创建可执行文件
add_executable(UploadClient ${ALL_SOURCES} ${ALL_HEADERS} ${UI_FILES})

# 设置目标属性
target_include_directories(UploadClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/FileInfo
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SyncUploadQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ThreadSafeRowLockQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/include
)

# 链接Qt6库
target_link_libraries(UploadClient 
    Qt6::Core 
    Qt6::Widgets
)

# 链接Windows系统库
if(WIN32)
    target_link_libraries(UploadClient Kernel32)
endif()

# 设置输出目录
set_target_properties(UploadClient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Windows特定设置
if(WIN32)
    set_target_properties(UploadClient PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# 编译器特定设置
if(MSVC)
    # 禁用一些警告
    target_compile_options(UploadClient PRIVATE /W3)
    # 设置字符集
    target_compile_definitions(UploadClient PRIVATE 
        UNICODE 
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
else()
    # GCC/Clang设置
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
