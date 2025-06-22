# High-performance file upload server

cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="D:/Qt/6.5.3/msvc2019_64" -DCMAKE_TOOLCHAIN_FILE=D:/cppsoft/vcpkg/scripts/buildsystems/vcpkg.cmake


cmake --build . --config Release


client :
    vcpkg install protobuf:x64-windows