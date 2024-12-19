file(GLOB SRC_FILES "*.c")

add_library(pv SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
# target_compile_definitions(pv PRIVATE STATISTICS)

target_compile_options(pv PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pv/=")
