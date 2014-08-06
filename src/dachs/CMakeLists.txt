# TODO: consider building below objects as shared object
file(GLOB_RECURSE CPPFILES *.cpp)
add_library(dachs-lib ${CPPFILES})
target_link_libraries(dachs-lib ${REQUIRED_LLVM_LIBRARIES} dachs-lib)
set_target_properties(dachs-lib PROPERTIES OUTPUT_NAME "dachs")
install(TARGETS dachs-lib ARCHIVE DESTINATION lib)
