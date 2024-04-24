cmake_minimum_required(VERSION 2.6)

project(cv CXX C)

set(CMAKE_C_FLAGS "-g")
set(CMAKE_CXX_FLAGS "-std=c++11 -g -O2")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/builds/cmake)

# src
set (cv_src 
            src/libs/image.cpp
            src/CV.cpp)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

add_library(cv_bin ${cv_src})

IF(LINUX)
	target_link_libraries(cv_bin -pthread  -m64  -lSDL2 -lSDL2main )
ENDIF(LINUX)

IF(MINGW)
	target_link_libraries(cv_bin -m64 -lSDL2 -lSDL2main -lopengl32 -lws2_32)
ENDIF(MINGW)


add_executable(cv src/Interpreter.cpp)
target_link_libraries(cv cv_bin)
