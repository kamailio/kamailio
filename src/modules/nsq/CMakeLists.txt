file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(Libev REQUIRED)

find_package(json-c QUIET)
if(NOT json-c_FOUND)
  message(STATUS "json-c not found. looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(json-c REQUIRED IMPORTED_TARGET json-c)
  add_library(json-c::json-c ALIAS PkgConfig::json-c)
endif()

find_package(CURL REQUIRED)

# TODO: find_library(libnsq nsq) TODO: * these libraries seems to be not
# maintaned anymore - eprecate? -lnsq -levbuffsock

# TODO: verify if -l is required to avoid linking to itself
target_link_libraries(${module_name} PRIVATE json-c::json-c CURL::libcurl
                                             Libev::Libev -lnsq evbuffsock)
