file(GLOB jsonrpcs_SRC "*.c")

add_library(jsonrpcs SHARED ${jsonrpcs_SRC})
# target_link_libraries(.*PRIVATE)

target_compile_options(jsonrpcs PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/jsonrpcs/=")
