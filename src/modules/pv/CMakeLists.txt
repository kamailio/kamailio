file(GLOB SRC_FILES "*.c")

add_library(pv SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE) target_compile_definitions(pv PRIVATE
# STATISTICS)
