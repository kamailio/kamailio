file(GLOB SRC_FILES "*.c")

add_library(alias_db SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(alias_db PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/alias_db/=")
