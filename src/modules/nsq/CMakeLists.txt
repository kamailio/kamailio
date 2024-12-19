find_package(Libev REQUIRED)

find_package(json-c QUIET)
if(NOT json-c_FOUND)
  message(STATUS "json-c not found. looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(json-c REQUIRED IMPORTED_TARGET json-c)
  add_library(json-c::json-c ALIAS PkgConfig::json-c)
endif()

find_package(CURL REQUIRED)

# find_library(libnsq nsq)

file(GLOB SRC_FILES "*.c")

# TODO: These libraries seems to be not maintaned anymore. Deprecate?
# -lnsq -levbuffsock
add_library(nsq SHARED ${SRC_FILES})

# TODO: verify -l is required to avoid linking to itself?
target_link_libraries(
  nsq PRIVATE json-c::json-c CURL::libcurl Libev::Libev -lnsq evbuffsock
)

target_compile_options(nsq PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/nsq/=")
