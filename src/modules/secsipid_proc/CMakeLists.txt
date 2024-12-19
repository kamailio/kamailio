find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(secsipid-1 REQUIRED IMPORTED_TARGET secsipid-1)
add_library(secsipid ALIAS PkgConfig::secsipid-1)

file(GLOB SRC_FILES "*.c")

add_library(secsipid_proc SHARED ${SRC_FILES})

if(APPLE)
  target_link_libraries(secsipid_proc PRIVATE "-framework CoreFoundation"
                                              "-framework Security")
endif()

target_link_libraries(secsipid_proc PRIVATE secsipid Threads::Threads)
