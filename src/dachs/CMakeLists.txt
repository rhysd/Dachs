# TODO: consider building below objects as shared object
find_package(Boost COMPONENTS system filesystem REQUIRED)
if (Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif ()

file(GLOB_RECURSE CPPFILES *.cpp)
add_library(dachs-lib ${CPPFILES})
target_link_libraries(dachs-lib ${REQUIRED_LLVM_LIBRARIES} ${Boost_LIBRARIES} dachs-lib)
set_target_properties(dachs-lib PROPERTIES OUTPUT_NAME "dachs")
install(TARGETS dachs-lib ARCHIVE DESTINATION lib)
