file(GLOB SRC_FILES "*.c")

add_library(db2_ops SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(db2_ops PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/db2_ops/=")
