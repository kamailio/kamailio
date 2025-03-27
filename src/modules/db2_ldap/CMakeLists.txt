file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# libsasl2-dev is required for LDAP support
find_package(PkgConfig REQUIRED)
pkg_check_modules(libsasl2 REQUIRED IMPORTED_TARGET libsasl2)
add_library(libsasl2::libsasl2 ALIAS PkgConfig::libsasl2)

# libldap-2.4-2 is required for LDAP support
find_package(Ldap REQUIRED)

# LDAP_DEPRECATED is required for LDAP support else it will throw compile error
target_compile_definitions(${module_name} PRIVATE LDAP_DEPRECATED)
target_include_directories(${module_name} PRIVATE ${Ldap_INCLUDE_DIRS})
target_link_libraries(${module_name} PRIVATE ${LDAP_LIBRARIES} libsasl2::libsasl2)
