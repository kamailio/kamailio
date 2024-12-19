find_package(CURL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xcap_client SHARED ${SRC_FILES})

target_link_libraries(xcap_client PRIVATE CURL::libcurl)
