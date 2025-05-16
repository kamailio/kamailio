file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(SQLite3 REQUIRED)

target_link_libraries(${module_name} PRIVATE SQLite::SQLite3)

include(${CMAKE_SOURCE_DIR}/cmake/db_files.cmake)
add_kamctl_db_files(${group_name} kamctl.sqlite)
add_kamdbctl_db_files(${group_name} kamdbctl.sqlite)
