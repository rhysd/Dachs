project(dachs)

set(DACHS_VERSION_MAJOR 0)
set(DACHS_VERSION_MINOR 0)
set(DACHS_VERSION_TEENY 1)
set(PACKAGE_VERSION "${DACHS_VERSION_MAJOR}.${DACHS_VERSION_MINOR}.${DACHS_VERSION_TEENY}")

cmake_minimum_required( VERSION 2.8 )

if(DACHS_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "${DACHS_CXX_COMPILER}")
elseif(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    message(STATUS "Default compiler is not clang.  Force to use clang.")
    if(EXISTS "/usr/local/bin/clang++-3.5")
        set(CMAKE_CXX_COMPILER "/usr/local/bin/clang++-3.5")
    elseif(EXISTS "/usr/local/bin/clang++-3.4")
        set(CMAKE_CXX_COMPILER "/usr/local/bin/clang++-3.4")
    elseif(EXISTS "/usr/bin/clang++")
        set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
    else()
        message(FATAL_ERROR "Clang is not found. Set -DCMAKE_CXX_COMPILER option")
    endif()
    message(STATUS "${CMAKE_CXX_COMPILER} is detected.")
endif()

include_directories( "${PROJECT_SOURCE_DIR}/src" )

add_definitions(-std=c++1y -g -Wall -Wextra)

find_package(Boost 1.55.0)
include_directories(${Boost_INCLUDE_DIRS})

file(GLOB_RECURSE CPPFILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
add_executable(dachs ${CPPFILES})

install(TARGETS dachs DESTINATION bin)
