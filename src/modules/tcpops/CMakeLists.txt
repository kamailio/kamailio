file(GLOB SRC_FILES "*.c")

add_library(tcpops SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(tcpops PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/tcpops/=")
