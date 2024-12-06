cmake_minimum_required(VERSION 3.10)
project(xhttp)

# Set the source files
file(GLOB xhttp_SRC *.c)

add_library(xhttp SHARED ${xhttp_SRC})
target_link_libraries(xhttp PRIVATE kamailio)
