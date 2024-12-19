find_package(Radius REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(misc_radius SHARED ${SRC_FILES})

target_compile_definitions(misc_radius PRIVATE ${RADIUSCLIENT_LIB})
# target_link_libraries(.*PRIVATE)

target_compile_options(misc_radius PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/misc_radius/=")
