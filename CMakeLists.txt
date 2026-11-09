cmake_minimum_required(VERSION 3.16)
project(OpenClip LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

add_executable(openclip
    src/main.cpp
    src/mainwindow.cpp
    src/timeline.cpp
    src/timelinewidget.cpp
    src/exporter.cpp
)
target_link_libraries(openclip PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)
