# TODO: consider about building below objects as shared object
file(GLOB_RECURSE CPPFILES *.cpp)
add_library(dachs-objects OBJECT ${CPPFILES})
