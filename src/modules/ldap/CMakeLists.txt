file(GLOB SRC_FILES "*.c")

find_package(Ldap REQUIRED)

add_library(ldap SHARED ${SRC_FILES})
target_link_libraries(ldap PRIVATE Ldap::Ldap)
