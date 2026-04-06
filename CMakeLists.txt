cmake_minimum_required(VERSION 3.10)

project(cv CXX C)

set(CMAKE_C_FLAGS "-g")
set(CMAKE_CXX_FLAGS "-std=c++17 -g")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/builds/cmake)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/)


# TODO: Install script should move the libraries to its respective directory in the system. Perhaps /usr/lib/canvasv/?

macro (mvLib t) 
    # Seems we only need to do this on Windows, GCC on Linux moves .sos to the lib directory
    IF(WIN32)
        add_custom_command(TARGET ${t} POST_BUILD COMMAND mv ./lib${t}.dll ./lib/)
    # ELSE()    
    #     add_custom_command(TARGET ${t} POST_BUILD COMMAND mv ./lib${t}.so ./lib/)
    ENDIF(WIN32)

endmacro()

set (cv_src 
            src/CV.cpp
            src/CLI.cpp)

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

# io
add_library(io SHARED src/libs/io.cpp)
target_link_libraries(io PRIVATE cv_bin)
mvLib(io)
# json
add_library(json SHARED src/libs/json.cpp src/Thirdparty/json11.cpp)
target_link_libraries(json PRIVATE cv_bin)
mvLib(json)
# file
add_library(file SHARED src/libs/file.cpp)
target_link_libraries(file PRIVATE cv_bin)
mvLib(file)
# math
add_library(math SHARED src/libs/math.cpp)
target_link_libraries(math PRIVATE cv_bin)
mvLib(math)
# time
add_library(time SHARED src/libs/time.cpp)
target_link_libraries(time PRIVATE cv_bin)
mvLib(time)