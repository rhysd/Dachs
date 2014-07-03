# TODO: consider building below objects as shared object
file(GLOB_RECURSE CPPFILES *.cpp)
add_library(dachs-lib ${CPPFILES})
set_target_properties(dachs-lib PROPERTIES OUTPUT_NAME "dachs")
