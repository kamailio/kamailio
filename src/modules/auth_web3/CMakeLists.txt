file(GLOB MODULE_SOURCES "*.c")
 
add_library(${module_name} SHARED ${MODULE_SOURCES})
 
find_package(CURL REQUIRED)
 
target_compile_definitions(${module_name} PRIVATE KAMAILIO_MOD_INTERFACE)
target_link_libraries(${module_name} PRIVATE CURL::libcurl)