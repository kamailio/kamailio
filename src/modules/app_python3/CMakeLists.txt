file(GLOB SRC_FILES "*.c")

find_package(Python3 REQUIRED COMPONENTS Development)

add_library(app_python3 SHARED ${SRC_FILES})
target_link_libraries(app_python3 PRIVATE Python3::Python)
