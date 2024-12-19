file(GLOB SRC_FILES "*.c")

add_library(uid_avp_db SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  uid_avp_db
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/uid_avp_db/=")
set_target_properties(uid_avp_db PROPERTIES SUFFIX ".so")
