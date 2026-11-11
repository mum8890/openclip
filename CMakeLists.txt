cmake_minimum_required(VERSION 3.16)
project(OpenClip LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(OPENCLIP_VERSION "0.6.0" CACHE STRING "App version (the GitHub release tag overrides this)")
# Safety net: a mangled version (like "0") would break the update popup, so refuse to build.
if(NOT OPENCLIP_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
    message(FATAL_ERROR "OPENCLIP_VERSION must look like 1.2.3 but is '${OPENCLIP_VERSION}'")
endif()
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Network)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/timeline.cpp
    src/timelinewidget.cpp
    src/exporter.cpp
    src/dialogs.cpp
    src/updater.cpp
    src/theme.cpp
    src/project.cpp
    src/templates.cpp
    resources/resources.qrc
)

if(WIN32)
    set(ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
    configure_file(resources/app.rc.in "${CMAKE_BINARY_DIR}/app.rc" @ONLY)
    list(APPEND SOURCES "${CMAKE_BINARY_DIR}/app.rc")
endif()

add_executable(openclip ${SOURCES})
set_target_properties(openclip PROPERTIES WIN32_EXECUTABLE ON)
target_compile_definitions(openclip PRIVATE OPENCLIP_VERSION="${OPENCLIP_VERSION}")
target_link_libraries(openclip PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets Qt6::Network)
