file(GLOB SRC_FILES "*.c")

add_library(sctp SHARED ${SRC_FILES})
if(UNIX)
  target_link_libraries(sctp PRIVATE -lsctp)
endif()
# target_link_libraries(.*PRIVATE)

target_compile_options(
  sctp PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/sctp/=")
set_target_properties(sctp PROPERTIES SUFFIX ".so")
