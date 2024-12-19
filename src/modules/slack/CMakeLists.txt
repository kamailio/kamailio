find_package(CURL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(slack SHARED ${SRC_FILES})
target_link_libraries(slack PRIVATE CURL::libcurl)
