file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# erlang-dev
find_package(Erlang REQUIRED)

target_link_libraries(${module_name} PRIVATE Erlang::EI)
