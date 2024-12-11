file(GLOB SRC_FILES "*.c")

add_library(xhttp_prom SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(xhttp_prom PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/xhttp_prom/=")
