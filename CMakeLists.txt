cmake_minimum_required(VERSION 3.10)

# Use cmake -DCMAKE_SYSTEM_NAME .. for cross-compiling (inside build directory)

# Set the project name
project(kamailio VERSION 6.0.0)

# Set the version number
set(EXTRAVERSION "-dev2")
set(RELEASE "${PROJECT_VERSION}${EXTRAVERSION}" )

message(STATUS "PROJECT_VERSION: ${PROJECT_VERSION}")
message(STATUS "RELEASE:  ${RELEASE}")

math(EXPR VERSIONVAL "${PROJECT_VERSION_MAJOR}*1000000 + ${PROJECT_VERSION_MINOR}*1000 + ${PROJECT_VERSION_PATCH}")
message(STATUS "VERSIONVAL: ${VERSIONVAL}")

# Specify the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Add the source directory
add_subdirectory(src)



# Add the executable
# add_executable(kamailio ${PROJECT_SOURCE_DIR}/src/main.c)
# target_link_libraries(kamailio PRIVATE core)
# target_link_libraries(kamailio full_path_to_generated_library)
# add_dependencies(kamailio kamailio_core)

# Link the libraries
# target_link_libraries(kamailio PRIVATE my_library)

# Add the install targets
# install(TARGETS kamailio DESTINATION bin)
# install(FILES "${PROJECT_BINARY_DIR}/TutorialConfig.h" DESTINATION include)

# # Add custom target for tags
# add_custom_target(tags
#   COMMAND ctags -R --exclude="misc/*" --exclude="test/*" .
#   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
# )

# # Add custom target for clean
# add_custom_target(distclean
#    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/clean.cmake
# )