file(GLOB SRC_FILES "*.c")

add_library(registrar SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE) target_compile_definitions(registrar PRIVATE
# STATISTICS)
