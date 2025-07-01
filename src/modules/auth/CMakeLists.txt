file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

target_compile_definitions(${module_name} PRIVATE USE_NC USE_OT_NONCE)
