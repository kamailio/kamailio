cmake_minimum_required(VERSION 3.10)

project(snmpstats)

# https://github.com/syslog-ng/syslog-ng/blob/1f6c3382ba69fafcc1876c5af840d1863213e95a/modules/snmp-dest/CMakeLists.txt
find_package(NETSNMP REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(snmpstats SHARED ${SRC_FILES})

# TODO: Compiles fines but need to do some more verifcation
target_link_libraries(snmpstats PRIVATE kamailio NETSNMP::NETSNMP)
