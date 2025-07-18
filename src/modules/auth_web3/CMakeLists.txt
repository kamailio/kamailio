file(GLOB MODULE_SOURCES "*.c")

find_package(CURL REQUIRED)

add_library(${module_name} SHARED ${MODULE_SOURCES})

target_compile_definitions(${module_name} PRIVATE USE_NC USE_OT_NONCE)

target_link_libraries(${module_name} PRIVATE CURL::libcurl) 