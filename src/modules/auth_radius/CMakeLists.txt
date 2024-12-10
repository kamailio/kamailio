file(GLOB SRC_FILES "*.c")

find_package(Radius REQUIRED)

add_library(auth_radius SHARED ${SRC_FILES})
target_compile_definitions(auth_radius PRIVATE ${RADIUSCLIENT_LIB})
# target_link_libraries(.*PRIVATE)
