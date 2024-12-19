file(GLOB SRC_FILES "*.c")

add_library(sms SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  sms PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/sms/=")
set_target_properties(sms PROPERTIES SUFFIX ".so")
