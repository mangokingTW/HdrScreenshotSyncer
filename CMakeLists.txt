cmake_minimum_required(VERSION 3.21)

project(HdrScreenshotSyncer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT so the exe runs on a clean Windows install with no VC++ redist
# (learned the hard way on the sibling project). Set before any target.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(HdrScreenshotSyncer WIN32
    src/main.cpp
    src/hdr.cpp
    src/hdr.h
    src/snip_setting.cpp
    src/snip_setting.h
    src/autostart.cpp
    src/autostart.h
)

target_link_libraries(HdrScreenshotSyncer PRIVATE user32 advapi32 shell32)

target_compile_definitions(HdrScreenshotSyncer PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_compile_options(HdrScreenshotSyncer PRIVATE /utf-8 /W4 /permissive-)
endif()
