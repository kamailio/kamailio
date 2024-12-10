file(GLOB jsonrpcs_SRC "*.c")

add_library(jsonrpcs SHARED ${jsonrpcs_SRC})
# target_link_libraries(.*PRIVATE)
