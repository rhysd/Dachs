file(GLOB_RECURSE CPPFILES *.cpp)

add_library(dachs-runtime ${CPPFILES})

install(TARGETS dachs-runtime ARCHIVE DESTINATION lib)
