add_subdirectory(dachs)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}")
add_executable(dachs main.cpp)
target_link_libraries(dachs dachs-lib)
install(TARGETS dachs RUNTIME DESTINATION bin)
