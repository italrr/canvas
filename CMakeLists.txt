cmake_minimum_required(VERSION 3.10)

project(cv CXX C)

set(CMAKE_C_FLAGS "-g")
set(CMAKE_CXX_FLAGS "-std=c++11 -g")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/builds/cmake)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/)


macro (mvLib t) 
    IF(WIN32)
        add_custom_command(TARGET ${t} POST_BUILD COMMAND mv ./lib${t}.dll ./lib/)
    ELSE()    
        add_custom_command(TARGET ${t} POST_BUILD COMMAND mv ./lib${t}.dll ./lib/)
    ENDIF(WIN32)

endmacro()

set (cv_src 
            src/CV.cpp
            src/Library.cpp
            src/Core.cpp)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

add_library(cv_bin ${cv_src})

IF(LINUX)
	target_link_libraries(cv_bin -pthread -m64)
ENDIF(LINUX)

IF(MINGW)
	target_link_libraries(cv_bin -m64)
ENDIF(MINGW)


add_executable(cv src/CLI.cpp)
target_link_libraries(cv cv_bin)

# LIBRARIES
# -------------------------------------------------------------------------- #

# bitmap
add_library(io SHARED src/libs/io.cpp src/CV.cpp src/Core.cpp src/Library.cpp)
mvLib(io)
# # time
# add_library(tm SHARED src/libs/time.cpp src/CV.cpp src/Tools.cpp)
# mvLib(tm)
# # math
# add_library(math SHARED src/libs/math.cpp src/CV.cpp src/Tools.cpp)
# mvLib(math)
# # io
# add_library(io SHARED src/libs/io.cpp src/CV.cpp src/Tools.cpp)
# mvLib(io)  