project(dachs)

set(DACHS_VERSION_MAJOR 0)
set(DACHS_VERSION_MINOR 0)
set(DACHS_VERSION_TEENY 1)
set(PACKAGE_VERSION "${DACHS_VERSION_MAJOR}.${DACHS_VERSION_MINOR}.${DACHS_VERSION_TEENY}")

cmake_minimum_required( VERSION 2.8 )

if(EXISTS "/usr/local/bin/clang++-3.5")
    set(CMAKE_CXX_COMPILER "clang++-3.5")
else()
    set(CMAKE_CXX_COMPILER "clang++")
endif()

include_directories( "${PROJECT_SOURCE_DIR}/src" )

add_definitions(-std=c++1y -g -Wall -Wextra)

find_package(Boost 1.55.0)
include_directories(${Boost_INCLUDE_DIRS})

file(GLOB_RECURSE CPPFILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
add_executable(dachs ${CPPFILES})

install(TARGETS dachs DESTINATION bin)
