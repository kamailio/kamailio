file(GLOB SRC_FILES "*.c")

find_package(Radius REQUIRED)

add_library(auth_radius SHARED ${SRC_FILES})
target_compile_definitions(auth_radius PRIVATE ${RADIUSCLIENT_LIB})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  auth_radius
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/auth_radius/=")
set_target_properties(auth_radius PROPERTIES SUFFIX ".so")
