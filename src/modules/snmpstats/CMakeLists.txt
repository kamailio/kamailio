file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# https://github.com/syslog-ng/syslog-ng/blob/1f6c3382ba69fafcc1876c5af840d1863213e95a/modules/snmp-dest/CMakeLists.txt
find_package(NETSNMP REQUIRED)

# TODO: Compiles fine but need to do some more verifcation
target_link_libraries(${module_name} PRIVATE NETSNMP::NETSNMP)
