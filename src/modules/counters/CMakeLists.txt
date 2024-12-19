file(GLOB SRC_FILES "*.c")

add_library(counters SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

# TODO: Also install kamcmd binary

target_compile_options(counters PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/counters/=")
