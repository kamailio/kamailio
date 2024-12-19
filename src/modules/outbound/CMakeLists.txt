find_package(OpenSSL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(outbound SHARED ${SRC_FILES})
target_link_libraries(outbound PRIVATE OpenSSL::SSL OpenSSL::Crypto)

target_compile_options(
  outbound
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/outbound/=")
set_target_properties(outbound PROPERTIES SUFFIX ".so")
