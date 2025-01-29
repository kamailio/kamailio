file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# TODO: Needs some testing for OS that support OracleDB
find_package(Oracle REQUIRED)

# /usr/include/oracle/18.3/client64)
target_include_directories(${module_name} PRIVATE ${ORACLE_INCLUDE_DIR})
target_link_libraries(${module_name} PRIVATE ${ORACLE_LIBRARIES})
