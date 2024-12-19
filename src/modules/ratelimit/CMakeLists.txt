file(GLOB SRC_FILES "*.c")

add_library(ratelimit SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(ratelimit PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ratelimit/=")
