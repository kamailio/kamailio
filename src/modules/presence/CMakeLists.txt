file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(LibXml2 REQUIRED)

# ICU is a required dependency for MacOS (darwin) for libxml2.
# https://github.com/kamailio/kamailio/commit/d3208e1859051eb09013f855126f7f5f5ec47ab8
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  find_package(ICU REQUIRED COMPONENTS uc)
  target_link_libraries(${module_name} PRIVATE ICU::uc)
endif()

target_link_libraries(${module_name} PRIVATE LibXml2::LibXml2)
