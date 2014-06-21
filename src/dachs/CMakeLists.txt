# TODO: consider building below objects as shared object
file(GLOB_RECURSE CPPFILES *.cpp)
add_library(dachs-lib ${CPPFILES})
