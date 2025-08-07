file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(OpenSSL REQUIRED)

# TODO: Verify if correct
if(EMBEDDED_UTF8_DECODE EQUAL 0)
  target_link_libraries(${module_name} PRIVATE unistring)
else()
  target_compile_definitions(${module_name} PRIVATE EMBEDDED_UTF8_DECODE)
endif()
