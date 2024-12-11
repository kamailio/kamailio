file(GLOB SRC_FILES "*.c")

add_library(app_jsdt SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(app_jsdt PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/app_jsdt/=")
