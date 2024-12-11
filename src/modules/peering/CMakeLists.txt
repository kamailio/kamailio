find_package(Radius REQUIRED)
file(GLOB SRC_FILES "*.c")

add_library(peering SHARED ${SRC_FILES})
target_compile_definitions(peering PRIVATE ${RADIUSCLIENT_LIB})

# target_link_libraries(.*PRIVATE)

target_compile_options(peering PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/peering/=")
