file(GLOB SRC_FILES "*.c")

find_package(PostgreSQL REQUIRED)

add_library(db_postgres SHARED ${SRC_FILES})

target_compile_definitions(db_postgres PRIVATE ${PostgreSQL_DEFINITIONS})
target_include_directories(db_postgres PRIVATE ${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(db_postgres PRIVATE ${PostgreSQL_LIBRARIES})
