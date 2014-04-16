set(Boost_USE_STATIC_LIBS OFF)
include_directories( "${PROJECT_SOURCE_DIR}/src" )
add_definitions(-std=c++1y -g -Wall -Wextra)

find_package(Boost COMPONENTS unit_test_framework REQUIRED)
if (Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif (Boost_FOUND)

add_executable(dachs-parser-test parser_test.cpp)
target_link_libraries(dachs-parser-test ${Boost_LIBRARIES})
add_test(dachs-parser-test ${EXECUTABLE_OUTPUT_PATH}/dachs-parser-test)
