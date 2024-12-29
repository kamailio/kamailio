file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PostgreSQL REQUIRED)

target_compile_definitions(${module_name} PRIVATE ${PostgreSQL_DEFINITIONS})
target_include_directories(${module_name} PRIVATE ${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(${module_name} PRIVATE ${PostgreSQL_LIBRARIES})
