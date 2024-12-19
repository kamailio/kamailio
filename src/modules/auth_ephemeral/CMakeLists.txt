file(GLOB SRC_FILES "*.c")

add_library(auth_ephemeral SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(auth_ephemeral PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/auth_ephemeral/=")
