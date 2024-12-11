file(GLOB SRC_FILES "*.c")

add_library(dmq_usrloc SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(dmq_usrloc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/dmq_usrloc/=")
