file(GLOB SRC_FILES "*.c")

add_library(presence_mwi SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(presence_mwi PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/presence_mwi/=")
