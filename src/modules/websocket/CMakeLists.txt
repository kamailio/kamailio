file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

option(EMBEDDED_UTF8_DECODE "Use embedded UTF-8 decode (websocket module)" OFF)

if(EMBEDDED_UTF8_DECODE)
  target_compile_definitions(${module_name} PRIVATE EMBEDDED_UTF8_DECODE)
else()
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_include_directories(${module_name} PRIVATE /opt/local/include)
    target_link_directories(${module_name} PRIVATE /opt/local/lib)
  endif()
  target_link_libraries(${module_name} PRIVATE unistring)
endif()
