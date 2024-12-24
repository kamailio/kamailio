file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(LibXml2 REQUIRED)

target_compile_definitions(${module_name} PRIVATE XMLRPC_SYSTEM_MALLOC)
target_link_libraries(${module_name} PRIVATE LibXml2::LibXml2)
