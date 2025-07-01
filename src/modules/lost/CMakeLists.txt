file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)

target_link_libraries(${module_name} PRIVATE CURL::libcurl LibXml2::LibXml2)
