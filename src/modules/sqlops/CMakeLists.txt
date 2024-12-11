file(GLOB SRC_FILES "*.c")

add_library(sqlops SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(sqlops PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/sqlops/=")
