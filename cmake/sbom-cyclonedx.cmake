# ----------
# sbom-cyclonedx target: generate a CycloneDX SBOM (JSON) describing the kamailio
# binary, the selected modules and the external libraries they link.
#
# The top-level CMakeLists.txt includes this file after add_subdirectory(src)
# so that the ADDED_MODULES_LIST global property and the kamailio/module
# targets exist. The SBOM is produced on demand with:
#   cmake --build <builddir> --target sbom-cyclonedx
# The linked-library inventory is taken from the built artifacts (ldd) and
# resolved to distribution packages, so the target depends on the build.
find_program(PYTHON3_EXECUTABLE NAMES python3 python QUIET)

if(NOT PYTHON3_EXECUTABLE)
  message(STATUS "python3 not found. Skip sbom CycloneDX target.")
  return()
endif()

get_property(sbom_modules GLOBAL PROPERTY ADDED_MODULES_LIST)
list(SORT sbom_modules)

# Git provenance. REPO_VER/REPO_HASH from src/CMakeLists.txt are scoped to
# src/, so query git again here (USE_GIT and GIT_EXECUTABLE are cached).
set(SBOM_REPO_HASH "unknown")
set(SBOM_REPO_VER "")
if(USE_GIT
   AND GIT_EXECUTABLE
   AND EXISTS "${PROJECT_SOURCE_DIR}/.git"
)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --verify --short=6 HEAD
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE SBOM_REPO_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    RESULT_VARIABLE sbom_git_dirty
  )
  if(sbom_git_dirty)
    set(SBOM_REPO_VER "${SBOM_REPO_HASH}-dirty")
  else()
    set(SBOM_REPO_VER "${SBOM_REPO_HASH}")
  endif()
endif()

if(sbom_modules)
  list(JOIN sbom_modules "\", \"" sbom_modules_joined)
  set(SBOM_MODULES_JSON "\"${sbom_modules_joined}\"")
else()
  set(SBOM_MODULES_JSON "")
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/sbom-cyclonedx-metadata.json.in
  ${CMAKE_BINARY_DIR}/sbom/sbom-metadata.cdx.json @ONLY
)

# List of built artifacts to inspect; generator expressions resolve the
# final paths (modules override PREFIX/SUFFIX).
set(sbom_artifacts "$<TARGET_FILE:kamailio>\n")
foreach(sbom_module IN LISTS sbom_modules)
  if(TARGET ${sbom_module})
    string(APPEND sbom_artifacts "$<TARGET_FILE:${sbom_module}>\n")
  endif()
endforeach()
file(
  GENERATE
  OUTPUT ${CMAKE_BINARY_DIR}/sbom/artifacts.txt
  CONTENT "${sbom_artifacts}"
)

add_custom_target(
  sbom-cyclonedx
  COMMAND
    ${PYTHON3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/sbom-cyclonedx-generate.py --metadata
    ${CMAKE_BINARY_DIR}/sbom/sbom-metadata.cdx.json --artifacts
    ${CMAKE_BINARY_DIR}/sbom/artifacts.txt --binary-dir ${CMAKE_BINARY_DIR} --output
    ${CMAKE_BINARY_DIR}/sbom/kamailio-sbom.cdx.json
  COMMENT "Generating CycloneDX SBOM in ${CMAKE_BINARY_DIR}/sbom/kamailio-sbom.cdx.json"
  VERBATIM
)
add_dependencies(sbom-cyclonedx kamailio)
if(TARGET modules)
  add_dependencies(sbom-cyclonedx modules)
endif()

install(
  FILES ${CMAKE_BINARY_DIR}/sbom/kamailio-sbom.cdx.json
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  COMPONENT kamailio-core
  OPTIONAL
)
