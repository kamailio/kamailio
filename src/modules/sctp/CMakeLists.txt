file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

if(UNIX)
  target_link_libraries(${module_name} PRIVATE -lsctp)
endif()
