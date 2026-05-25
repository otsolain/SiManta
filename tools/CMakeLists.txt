cmake_minimum_required(VERSION 3.16)
project(simanta_tools LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network)

# Existing icon converter
add_executable(png2ico png2ico.cpp)
target_link_libraries(png2ico PRIVATE Qt6::Core Qt6::Gui)

# Stress test simulator -- spawns N fake students against a Simanta teacher.
add_executable(stress_test stress_test.cpp)
target_link_libraries(stress_test PRIVATE Qt6::Core Qt6::Gui Qt6::Network)
