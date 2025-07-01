file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# TODO: library libval not foud on recent distributions: whichi library is sres?
# Consider depreciating this module?
target_link_libraries(
  ${module_name} PRIVATE val-threads sres Threads::Threads OpenSSL::SSL OpenSSL::Crypto
)
