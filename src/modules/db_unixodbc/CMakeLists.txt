file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(ODBC REQUIRED)

target_include_directories(${module_name} PRIVATE ${ODBC_INCLUDE_DIRS})
target_link_libraries(${module_name} PRIVATE ${ODBC_LIBRARIES})
