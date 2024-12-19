file(GLOB SRC_FILES "*.c")

add_library(pua_rpc SHARED ${SRC_FILES})
target_compile_definitions(pua_rpc PRIVATE WITH_EVENT_LOCAL_REQUEST)
# target_link_libraries(.*PRIVATE)

target_compile_options(
  pua_rpc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pua_rpc/=")
set_target_properties(pua_rpc PROPERTIES SUFFIX ".so")
