file(GLOB SRC_FILES "*.c")

add_library(prefix_route SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(prefix_route PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/prefix_route/=")
