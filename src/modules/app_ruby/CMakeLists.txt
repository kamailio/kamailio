file(GLOB SRC_FILES "*.c")

find_package(Ruby REQUIRED)

add_library(app_ruby SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
