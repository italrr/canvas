cmake_minimum_required(VERSION 2.6)

project(cv CXX C)

set(CMAKE_C_FLAGS "-g")
set(CMAKE_CXX_FLAGS "-std=c++11 -g")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/builds/cmake)

# src
set (cv_src 
           src/cv.cpp)

add_library(cv_bin ${cv_src})
target_link_libraries(cv_bin -pthread)

add_executable(cv src/repl.cpp)
target_link_libraries(cv cv_bin)