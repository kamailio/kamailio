set(module_name "auth_arnacon")

file(GLOB MODULE_SOURCES "*.c")

find_package(CURL REQUIRED)

# Try to find secp256k1 using pkg-config
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SECP256K1 libsecp256k1)
endif()

add_library(${module_name} MODULE ${MODULE_SOURCES})

target_compile_definitions(${module_name} PRIVATE KAMAILIO_MOD_INTERFACE)
target_include_directories(${module_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries
if(SECP256K1_FOUND)
    target_include_directories(${module_name} PRIVATE ${SECP256K1_INCLUDE_DIRS})
    target_link_libraries(${module_name} PRIVATE CURL::libcurl ${SECP256K1_LIBRARIES})
else()
    # Fallback: link directly with -lsecp256k1 (standard system library)
    target_link_libraries(${module_name} PRIVATE CURL::libcurl secp256k1)
endif()

# Register the module with the Kamailio build helpers
add_kamailio_module(${module_name})

