cmake_minimum_required(VERSION 3.10)

project(cv CXX C)

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/builds/cmake)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Release C flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Release C++ flags" FORCE)

option(CV_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)

set(CV_INSTALL_MODULEDIR "${CMAKE_INSTALL_LIBDIR}/canvas" CACHE PATH "Install dir for Canvas modules")

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

# io
add_library(io SHARED
    src/libs/io.cpp
)
mvLib(io)

# math
add_library(math SHARED
    src/libs/math.cpp
)
mvLib(math)

# time
add_library(time SHARED
    src/libs/time.cpp
)
mvLib(time)

# file
add_library(file SHARED
    src/libs/file.cpp
)
mvLib(file)

if(CV_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(json PRIVATE ${CV_SANITIZER_COMPILE_FLAGS})
    target_link_options(json PRIVATE ${CV_SANITIZER_LINK_FLAGS})

    target_compile_options(io PRIVATE ${CV_SANITIZER_COMPILE_FLAGS})
    target_link_options(io PRIVATE ${CV_SANITIZER_LINK_FLAGS})

    target_compile_options(math PRIVATE ${CV_SANITIZER_COMPILE_FLAGS})
    target_link_options(math PRIVATE ${CV_SANITIZER_LINK_FLAGS})

    target_compile_options(time PRIVATE ${CV_SANITIZER_COMPILE_FLAGS})
    target_link_options(time PRIVATE ${CV_SANITIZER_LINK_FLAGS})

    target_compile_options(file PRIVATE ${CV_SANITIZER_COMPILE_FLAGS})
    target_link_options(file PRIVATE ${CV_SANITIZER_LINK_FLAGS})
endif()

# INSTALL
# -------------------------------------------------------------------------- #

install(TARGETS cv
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS json io math time file
    LIBRARY DESTINATION ${CV_INSTALL_MODULEDIR}
    RUNTIME DESTINATION ${CV_INSTALL_MODULEDIR}
)