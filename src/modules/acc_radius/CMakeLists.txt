file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(Radius REQUIRED)

target_compile_definitions(${module_name} PRIVATE ${RADIUSCLIENT_LIB})
target_link_libraries(${module_name} PRIVATE RadiusClient::RadiusClient)
