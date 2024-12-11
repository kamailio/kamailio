file(GLOB SRC_FILES "*.c")

add_library(uac_redirect SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(uac_redirect PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/uac_redirect/=")
