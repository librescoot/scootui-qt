cmake_minimum_required(VERSION 3.16)
project(dbc-media-viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

qt_add_executable(dbc-media-viewer main.cpp)
target_link_libraries(dbc-media-viewer PRIVATE Qt6::Widgets)
install(TARGETS dbc-media-viewer RUNTIME DESTINATION bin)
