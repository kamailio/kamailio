file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

target_compile_definitions(${module_name} PRIVATE WITH_EVENT_LOCAL_REQUEST)
