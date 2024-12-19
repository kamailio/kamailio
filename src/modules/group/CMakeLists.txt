file(GLOB SRC_FILES "*.c")

add_library(group SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(group PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/group/=")
