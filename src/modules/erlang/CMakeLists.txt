file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# erlang-dev
find_package(Erlang REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(${module_name} PRIVATE Erlang::Erlang Erlang::EI Threads::Threads)
target_include_directories(${module_name} PRIVATE ${Erlang_INCLUDE_DIRS})
target_compile_definitions(${module_name} PRIVATE _REENTRANT)
