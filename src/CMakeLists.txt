add_subdirectory(dachs)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}")
add_executable(dachs main.cpp $<TARGET_OBJECTS:dachs-objects>)
target_link_libraries(dachs ${REQUIRED_LLVM_LIBRARIES})
install(TARGETS dachs DESTINATION bin)
