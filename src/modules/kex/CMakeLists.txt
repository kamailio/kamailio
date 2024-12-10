file(GLOB SRC_FILES *.c)

add_library(kex SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
# target_include_directories(kex PUBLIC ${CMAKE_SOURCE_DIR}/src/core)
