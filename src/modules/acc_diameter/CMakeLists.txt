file(GLOB SRC_FILES "*.c")

add_library(acc_diameter SHARED ${SRC_FILES})
