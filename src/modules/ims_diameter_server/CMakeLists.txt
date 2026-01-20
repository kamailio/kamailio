file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(${module_name} PRIVATE Threads::Threads)

find_library(LIBRT rt)
if(LIBRT)
  target_link_libraries(${module_name} PRIVATE ${LIBRT})
endif()
