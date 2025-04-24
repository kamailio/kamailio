# Retrieve all defined components
get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)

message(STATUS "All components: ${CPACK_COMPONENTS_ALL}")
# Optionally filter components
# list(FILTER CPACK_COMPONENTS_ALL INCLUDE REGEX "^(core|tls)$")

set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
# set(CPACK_PACKAGE_NAME kamailio)
set(CPACK_DEBIAN_PACKAGE_NAME kamailio)
set(CPACK_GENERATOR DEB)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "test")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

# Configure CPack with the selected components
include(CPack)
