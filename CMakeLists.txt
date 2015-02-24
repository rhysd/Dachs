project(dachs)

set(DACHS_VERSION_MAJOR 0)
set(DACHS_VERSION_MINOR 0)
set(DACHS_VERSION_TEENY 1)
set(PACKAGE_VERSION "${DACHS_VERSION_MAJOR}.${DACHS_VERSION_MINOR}.${DACHS_VERSION_TEENY}")

cmake_minimum_required(VERSION 2.8.8)
enable_testing()

# Detect commands {{{
if (DACHS_CXX_COMPILER)
    if (NOT DACHS_LLVM_CONFIG)
        message(FATAL_ERROR "Set -DDACHS_LLVM_CONFIG to specify the path to your llvm-config command.")
    endif ()
    set(CMAKE_CXX_COMPILER "${DACHS_CXX_COMPILER}")

    message(STATUS "${CMAKE_CXX_COMPILER} is specified.")
else ()
    message(STATUS "Detecting a compiler for Dachs...")
    if (EXISTS "/usr/local/bin/clang++-3.5")
        set(CMAKE_CXX_COMPILER "/usr/local/bin/clang++-3.5")
        set(DACHS_CLANG_SUFFIX "-3.5")
    elseif (EXISTS "/usr/local/bin/clang++-3.4")
        set(CMAKE_CXX_COMPILER "/usr/local/bin/clang++-3.4")
        set(DACHS_CLANG_SUFFIX "-3.4")
    elseif (EXISTS "/usr/bin/clang++-3.5")
        set(CMAKE_CXX_COMPILER "/usr/bin/clang++-3.5")
        set(DACHS_CLANG_SUFFIX "-3.5")
    elseif (EXISTS "/usr/bin/clang++-3.4")
        set(CMAKE_CXX_COMPILER "/usr/bin/clang++-3.4")
        set(DACHS_CLANG_SUFFIX "-3.4")
    elseif (EXISTS "/usr/bin/clang++")
        set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
        set(DACHS_CLANG_SUFFIX "")
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(DACHS_CLANG_SUFFIX "")
    else ()
        message(FATAL_ERROR "Clang is not found. Set -DCMAKE_CXX_COMPILER option.")
    endif ()

    message(STATUS "${CMAKE_CXX_COMPILER} is detected.")
endif ()


if (NOT DACHS_LLVM_CONFIG)
    set(DACHS_LLVM_CONFIG "llvm-config${DACHS_CLANG_SUFFIX}")
    if (EXISTS "/usr/bin/${DACHS_LLVM_CONFIG}")
        set(DACHS_LLVM_CONFIG "/usr/bin/${DACHS_LLVM_CONFIG}")
    elseif (EXISTS "/usr/local/bin/${DACHS_LLVM_CONFIG}")
        set(DACHS_LLVM_CONFIG "/usr/local/bin/${DACHS_LLVM_CONFIG}")
    else ()
        message(FATAL_ERROR "'${DACHS_LLVM_CONFIG}' command is not found. Set -DDACHS_LLVM_CONFIG option.")
    endif ()
    message(STATUS "${DACHS_LLVM_CONFIG} is detected.")
else ()
    if (NOT EXISTS "${DACHS_LLVM_CONFIG}")
        message(FATAL_ERROR "'${DACHS_LLVM_CONFIG}' command is not found. Set -DDACHS_LLVM_CONFIG option properly.")
    endif ()
endif ()
# }}}

# LLVM configurations {{{
execute_process(
    COMMAND
    ${DACHS_LLVM_CONFIG} --version
    OUTPUT_VARIABLE
    LLVM_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
execute_process(
    COMMAND
    ${DACHS_LLVM_CONFIG} --includedir
    OUTPUT_VARIABLE
    LLVM_INCLUDE_DIRS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
execute_process(
    COMMAND
    ${DACHS_LLVM_CONFIG} --libdir
    OUTPUT_VARIABLE
    LLVM_LIBRARY_DIRS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )

execute_process(
    COMMAND
    ${DACHS_LLVM_CONFIG} --libnames
    OUTPUT_VARIABLE
    LLVM_LIBRARY_NAMES_STRING
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
string(REGEX REPLACE " +" ";" LLVM_LIBRARY_NAMES ${LLVM_LIBRARY_NAMES_STRING})
string(REGEX REPLACE "lib([^.]*)\\.a" "\\1" LLVM_LIBRARY_NAMES ${LLVM_LIBRARY_NAMES})

execute_process(
    COMMAND
    ${DACHS_LLVM_CONFIG} --components
    OUTPUT_VARIABLE
    LLVM_COMPONENTS_STRING
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
string(REGEX MATCHALL "([^ ]+)" LLVM_COMPONENTS ${LLVM_COMPONENTS_STRING})

set(DACHS_LLVM_COMPONENTS
    core
    support
    native
    asmparser
    asmprinter
    ipo
    )

foreach (c ${DACHS_LLVM_COMPONENTS})
    list(FIND LLVM_COMPONENTS ${c} found)
    if (found EQUAL -1)
        message(FATAL_ERROR "LLVM component '${c}' is not supported.")
    endif ()
endforeach (c)

execute_process(
    COMMAND
    ${DACHS_LLVM_CONFIG} --ldflags
    OUTPUT_VARIABLE
    LLVM_SYS_LIBS_STRING
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
string(REGEX MATCHALL "-l([^ ]+)" LLVM_SYS_LIBS ${LLVM_SYS_LIBS_STRING})

# In Ubuntu, somehow references to LLVM objects are not found in static link libraries.
# Because of workaround for this, use shared library if DACHS_USE_LLVM_SHARED_LIB is specified
if(DACHS_USE_LLVM_SHARED_LIB)
    foreach(l "LLVM" "LLVM-3.4" "LLVM-3.5")
        if (EXISTS "${LLVM_LIBRARY_DIRS}/lib${l}.so")
            set(LLVM_LIBS ${l})
            break()
        endif ()
    endforeach(l)
else()
    execute_process(
        COMMAND
        ${DACHS_LLVM_CONFIG} --libs ${DACHS_LLVM_COMPONENTS}
        OUTPUT_VARIABLE
        LLVM_LIBS_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    string(REGEX MATCHALL "-l([^ ]+)" LLVM_LIBS ${LLVM_LIBS_STRING})
endif()

set(REQUIRED_LLVM_LIBRARIES)
foreach (l ${LLVM_LIBS} ${LLVM_SYS_LIBS})
    string(REGEX REPLACE "-l([^ ]+)" "\\1" lib_name ${l})
    list(APPEND REQUIRED_LLVM_LIBRARIES ${lib_name})
endforeach (l)

# Check libraries which LLVM depends on
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND REQUIRED_LLVM_LIBRARIES "z" "pthread" "dl" "curses")
else ()
  # FIXME
  message(FATAL_ERROR "Currently, your platform ${CMAKE_SYSTEM_NAME} is not supported...")
endif ()

include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

message(STATUS "LLVM version    : ${LLVM_VERSION}")
message(STATUS "LLVM includedir : ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM libdir     : ${LLVM_LIBRARY_DIRS}")

set(LLVM_DEFINITIONS
    -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
    )
set(DACHS_DEFINITIONS
    -DDACHS_CXX_COMPILER="${CMAKE_CXX_COMPILER}" -DDACHS_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}"
    )
add_definitions(${LLVM_DEFINITIONS})
add_definitions(${DACHS_DEFINITIONS})

# }}}

include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/runtime/src")
find_package(Boost 1.55.0)
include_directories(${Boost_INCLUDE_DIRS})
add_definitions(-std=c++1y -Wall -Wextra)
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
    message(STATUS "Dachs is under development. 'Debug' is set to $CMAKE_BUILD_TYPE as default.")
    add_definitions("${CMAKE_CXX_FLAGS_DEBUG}")
endif ()

add_subdirectory(src)
add_subdirectory(runtime)
add_subdirectory(test)
