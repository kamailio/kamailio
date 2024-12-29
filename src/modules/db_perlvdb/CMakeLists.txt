file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PerlLibs REQUIRED)

target_include_directories(${module_name} PRIVATE ${PERL_INCLUDE_PATH})
target_compile_definitions(${module_name} PRIVATE ${PERL_EXTRA_C_FLAGS})
target_link_libraries(${module_name} PRIVATE ${PERL_LIBRARY})
