file(GLOB SRC_FILES "*.c")

find_package(Ruby REQUIRED)

add_library(app_ruby SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(app_ruby PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/app_ruby/=")
