set(Boost_USE_STATIC_LIBS OFF)
include_directories( "${PROJECT_SOURCE_DIR}/src" )
add_definitions(-std=c++1y -g -Wall -Wextra)

add_executable(dachs-parser-test parser_test.cpp)

find_package(Boost COMPONENTS system filesystem REQUIRED)
if (Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif ()
target_link_libraries(dachs-parser-test ${Boost_LIBRARIES} dachs-lib)

if (NOT APPLE)
    # Do not link library explicitly in Mac because it causes SEGV on executing tests.
    # Somehow this bug isn't reproduced when the library is not linked explicitry.
    # https://svn.boost.org/trac/boost/ticket/7512
    find_package(Boost COMPONENTS unit_test_framework REQUIRED)
    target_link_libraries(dachs-parser-test ${Boost_LIBRARIES})
endif()

add_test(dachs-parser-test ${EXECUTABLE_OUTPUT_PATH}/dachs-parser-test)
