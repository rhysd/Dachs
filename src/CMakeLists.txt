add_subdirectory(dachs)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}")
add_executable(dachs main.cpp)
target_link_libraries(dachs ${REQUIRED_LLVM_LIBRARIES} dachs-lib)
install(TARGETS dachs DESTINATION bin)
