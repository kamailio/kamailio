file(GLOB SRC_FILES "*.c")

add_library(keepalive SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(keepalive PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/keepalive/=")
