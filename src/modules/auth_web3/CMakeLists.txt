set(module_name "auth_web3")

file(GLOB MODULE_SOURCES "*.c")

find_package(CURL REQUIRED)

add_library(${module_name} MODULE ${MODULE_SOURCES})

target_compile_definitions(${module_name} PRIVATE KAMAILIO_MOD_INTERFACE)
target_include_directories(${module_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${module_name} PRIVATE CURL::libcurl)
