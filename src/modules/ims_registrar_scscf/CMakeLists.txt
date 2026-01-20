file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(LibXml2 REQUIRED)
find_package(Threads REQUIRED)

find_library(LIBRT rt)
if(LIBRT)
  target_link_libraries(${module_name} PRIVATE ${LIBRT})
endif()

target_link_libraries(${module_name} PRIVATE LibXml2::LibXml2 Threads::Threads)
