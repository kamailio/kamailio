file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(LibXml2 REQUIRED)

# TODO: Only for darwin?
find_package(ICU QUIET COMPONENTS uc)

target_link_libraries(${module_name} PRIVATE LibXml2::LibXml2 ICU::uc)
