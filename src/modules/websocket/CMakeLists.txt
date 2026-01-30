file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

option(EMBEDDED_UTF8_DECODE "Use embedded UTF-8 decode (websocket module)" OFF)

if(EMBEDDED_UTF8_DECODE)
  target_compile_definitions(${module_name} PRIVATE EMBEDDED_UTF8_DECODE)
else()
  find_package(Unistring REQUIRED)
  target_link_libraries(${module_name} PRIVATE Unistring::Unistring)
endif()
