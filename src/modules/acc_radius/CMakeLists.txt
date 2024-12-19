file(GLOB SRC_FILES "*.c")

find_package(Radius REQUIRED)

add_library(acc_radius SHARED ${SRC_FILES})
target_compile_definitions(acc_radius PRIVATE ${RADIUSCLIENT_LIB})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  acc_radius
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/acc_radius/=")
set_target_properties(acc_radius PROPERTIES SUFFIX ".so")
