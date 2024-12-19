file(GLOB SRC_FILES "*.c")

add_library(call_obj SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(call_obj PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/call_obj/=")
