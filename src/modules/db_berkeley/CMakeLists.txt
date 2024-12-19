# libdb++-dev
find_package(BerkeleyDB REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(db_berkeley SHARED ${SRC_FILES})
target_link_libraries(db_berkeley PRIVATE BerkeleyDB::BerkeleyDB)
