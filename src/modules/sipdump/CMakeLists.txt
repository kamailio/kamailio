file(GLOB SRC_FILES "*.c")

add_library(sipdump SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(sipdump PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/sipdump/=")
