cmake_minimum_required(VERSION 3.21)

project(HdrScreenshotSyncer VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(APP_VERSION "${PROJECT_VERSION}")
string(REPLACE "." ";" _ver_parts "${APP_VERSION}")
list(GET _ver_parts 0 APP_VERSION_MAJOR)
list(GET _ver_parts 1 APP_VERSION_MINOR)
list(GET _ver_parts 2 APP_VERSION_PATCH)
configure_file(src/resources.rc.in "${CMAKE_CURRENT_BINARY_DIR}/resources.rc" @ONLY)

# Static CRT so the exe runs on a clean Windows install with no VC++ redist
# (learned the hard way on the sibling project). Set before any target.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(HdrScreenshotSyncer WIN32
    src/main.cpp
    src/hdr.cpp
    src/hdr.h
    src/hdr_content.cpp
    src/hdr_content.h
    src/snip_setting.cpp
    src/snip_setting.h
    src/autostart.cpp
    src/autostart.h
    src/diagnostic.cpp
    src/diagnostic.h
    src/overrides_dialog.cpp
    src/overrides_dialog.h
    src/strings.cpp
    src/strings.h
    src/resource.h
    "${CMAKE_CURRENT_BINARY_DIR}/resources.rc"
)

target_link_libraries(HdrScreenshotSyncer PRIVATE user32 advapi32 shell32 comdlg32 d3d11 dxgi windowsapp)

target_compile_definitions(HdrScreenshotSyncer PRIVATE UNICODE _UNICODE APP_VERSION_STRING="${APP_VERSION}")

if(MSVC)
    target_compile_options(HdrScreenshotSyncer PRIVATE /utf-8 /W4 /permissive-)
endif()
