file(GLOB auth_SRC "*.c")

add_library(auth SHARED ${auth_SRC})
# target_link_libraries(.*PRIVATE)
target_compile_definitions(auth PRIVATE USE_NC USE_OT_NONCE)

target_compile_options(
  auth PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/auth/=")
set_target_properties(auth PROPERTIES SUFFIX ".so")
