file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(Ruby REQUIRED)
find_package(Threads REQUIRED)

target_include_directories(${module_name} PRIVATE ${RUBY_INCLUDE_DIRS})
target_link_libraries(${module_name} PRIVATE ${RUBY_LIBRARY} Threads::Threads ${CMAKE_DL_LIBS})
