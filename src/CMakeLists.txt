add_subdirectory(dachs)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}")
add_executable(dachs main.cpp $<TARGET_OBJECTS:dachs-objects>)
install(TARGETS dachs DESTINATION bin)
