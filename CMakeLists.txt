cmake_minimum_required(VERSION 3.10)

project(cv CXX C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/builds/cmake)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(CV_ENABLE_SANITIZERS "Enable ASan/UBSan" ON)

# TODO: Install script should move the libraries to its respective directory in the system. Perhaps /usr/lib/canvasv/?

macro(mvLib t)
    # Seems we only need to do this on Windows, GCC on Linux moves .sos to the lib directory
    if(WIN32)
        add_custom_command(TARGET ${t} POST_BUILD COMMAND mv ./lib${t}.dll ./lib/)
    endif()
endmacro()

set(cv_src
    src/CV.cpp
)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# Core runtime library
add_library(cv_bin ${cv_src})

if(LINUX)
    target_link_libraries(cv_bin PRIVATE pthread -m64)
endif()

if(MINGW)
    target_link_libraries(cv_bin PRIVATE -m64)
endif()

# Sanitizers
if(CV_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CV_SANITIZER_COMPILE_FLAGS
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -fno-sized-deallocation
        -g3
    )

    set(CV_SANITIZER_LINK_FLAGS
        -fsanitize=address,undefined
    )

    target_compile_options(cv_bin PRIVATE ${CV_SANITIZER_COMPILE_FLAGS})
    target_link_options(cv_bin PRIVATE ${CV_SANITIZER_LINK_FLAGS})
endif()

# Main executable
add_executable(cv src/CLI.cpp)
target_link_libraries(cv PRIVATE cv_bin)

set_target_properties(cv PROPERTIES ENABLE_EXPORTS ON)

if(CV_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cv PRIVATE ${CV_SANITIZER_COMPILE_FLAGS})
    target_link_options(cv PRIVATE
        ${CV_SANITIZER_LINK_FLAGS}
        -Wl,--export-dynamic
    )
else()
    target_link_options(cv PRIVATE -Wl,--export-dynamic)
endif()

# LIBRARIES
# -------------------------------------------------------------------------- #

# json
add_library(json SHARED
    src/libs/json.cpp
    src/Thirdparty/json11.cpp
)
mvLib(json)

# json
add_library(bmp SHARED
    src/libs/bmp.cpp
)
mvLib(bmp)

if(CV_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(json PRIVATE ${CV_SANITIZER_COMPILE_FLAGS})
    target_link_options(json PRIVATE ${CV_SANITIZER_LINK_FLAGS})
    target_compile_options(bmp PRIVATE ${CV_SANITIZER_COMPILE_FLAGS})
    target_link_options(bmp PRIVATE ${CV_SANITIZER_LINK_FLAGS})    
endif()