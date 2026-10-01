cmake_minimum_required(VERSION 3.20)
project(Bootelware VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime (/MT) for Release builds to ensure single portable .exe
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(SOURCES
    src/main.cpp
    src/core/system_detector.cpp
    src/core/device_manager.cpp
    src/core/checksum.cpp
    src/core/iso_reader.cpp
    src/core/win11_bypass.cpp
    src/core/disk_writer.cpp
    src/ui/dark_theme.cpp
    src/ui/dialogs.cpp
    src/ui/main_window.cpp
    resources/bootelware.rc
    resources/app.manifest
)

add_executable(Bootelware WIN32 ${SOURCES})

target_include_directories(Bootelware PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

target_link_libraries(Bootelware PRIVATE
    dwmapi
    virtdisk
    bcrypt
    comctl32
    setupapi
    msimg32
    gdiplus
)

# Compiler optimization options
if(MSVC)
    target_compile_options(Bootelware PRIVATE /W3 /permissive- /utf-8)
    set_target_properties(Bootelware PROPERTIES
        LINK_FLAGS "/MANIFESTUAC:\"level='asInvoker' uiAccess='false'\""
    )
endif()
