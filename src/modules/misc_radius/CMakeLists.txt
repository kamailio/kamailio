find_package(Radius REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(misc_radius SHARED ${SRC_FILES})

target_compile_definitions(misc_radius PRIVATE ${RADIUSCLIENT_LIB})
# target_link_libraries(.*PRIVATE)
