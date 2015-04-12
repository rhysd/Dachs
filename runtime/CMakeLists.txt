file(GLOB_RECURSE CPPFILES *.cpp)

add_library(dachs-runtime ${CPPFILES})

find_path(LIBGC_INCLUDE_DIR gc.h)
if (IS_DIRECTORY ${LIBGC_INCLUDE_DIR})
    message(FATAL_INFO "libgc was found: ${LIBGC_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "libgc is not found")
endif()

include_directories(${LIBGC_INCLUDE_DIR})
target_link_libraries(dachs-runtime "${LIBGC_INCLUDE_DIR}../lib")

install(TARGETS dachs-runtime ARCHIVE DESTINATION lib)
