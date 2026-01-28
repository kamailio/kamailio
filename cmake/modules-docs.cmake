option(DOCS_XSL_VAIDATION "Docbook document validation" OFF)
option(DOCS_NOCATALOG "ON: Use standard catalog from OS | OFF: Use custom catalog " OFF)

set(DOCS_SOURCES
    "index.html"
    CACHE STRING "Documentation files list"
)

set(DOCS_README
    ${DOCS_SOURCES}
    CACHE STRING "Readme Documentation files list"
)

set(DOCS_HTML
    ${DOCS_SOURCES}
    CACHE STRING "HTML Documentation files list"
)

set(DOCS_TXT
    ${DOCS_SOURCES}
    CACHE STRING "TXT Documentation files list"
)

set(DOCS_OUTPUT_DIR
    ${CMAKE_BINARY_DIR}/doc/docbook
    CACHE STRING "Path to build HTML docs"
)

set(DOCBOOK_DIR ${CMAKE_SOURCE_DIR}/doc/docbook)
set(STYLESHEET_DIR ${CMAKE_SOURCE_DIR}/doc/stylesheets)
set(DEPS_XSL ${DOCBOOK_DIR}/dep.xsl)
set(SINGLE_HTML_XSL ${DOCBOOK_DIR}/html.xsl)
set(CHUNKED_HTML_XSL ${DOCBOOK_DIR}/html.chunked.xsl)
set(TXT_XSL ${DOCBOOK_DIR}/txt.xsl)
set(README_XSL ${DOCBOOK_DIR}/readme.xsl)

set(DOCS_HTML_CSS
    "/css/sr-doc.css"
    CACHE STRING "Path to the CSS file"
)

set(CATALOG ${DOCBOOK_DIR}/catalog.xml)

if(DOCS_NOCATALOG)
  set(XMLCATATLOGX "")
else()
  set(XMLCATATLOGX "XML_CATALOG_FILES=${CATALOG}")
endif()

# Set flags for xtproc for generating documentation and allow user defined
set(DOCS_XSLTPROC_FLAGS
    ""
    CACHE STRING "Xsltransform processor flags"
)
if(NOT DOCS_XSL_VAIDATION)
  if(VERBOSE)
    message(STATUS "DOCS_XSL_VAIDATION=OFF"
                   "Disabling xsl validation when generating documentation"
    )
  endif()
  list(APPEND DOCS_XSLTPROC_FLAGS --novalid)
endif()

# Set lynx flags for generating readmes and allow user defined
set(DOCS_LYNX_FLAGS
    "-nolist"
    CACHE STRING "Lynx readme generator flags"
)

# Function to add a module docs entry
function(docs_add_module group_name module_name)
  # message(STATUS "Adding documentation for module ${MODULE_NAME}")
  set(module_path "${MODULES_DIR}/${module_name}")
  set(module_doc_path "${module_path}/doc")

  add_custom_target(
    ${module_name}_doc_text
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${module_name}/${module_name}.txt
    COMMENT "Processing target ${module_name}_doc_text"
  )

  # This is essentialy an alias of doc_text target but with extra copy command
  # to copy the text file to the source tree directory.
  add_custom_target(${module_name}_readme COMMENT "Processing target ${module_name}_readme")
  add_dependencies(${module_name}_readme ${module_name}_doc_text)
  add_dependencies(kamailio_docs_readme ${module_name}_readme)

  add_custom_target(
    ${module_name}_doc_html
    DEPENDS ${DOCS_OUTPUT_DIR}/${module_name}.html
    COMMENT "Processing target ${module_name}_doc_html"
  )

  add_custom_target(${module_name}_doc COMMENT "Processing target ${module_name}_doc")
  add_dependencies(${module_name}_doc ${module_name}_doc_text ${module_name}_doc_html)

  # Each version has seperate custon commands for not recompiling all if 1 gets
  # changed.
  # if(XSLTPROC_EXECUTABLE)
  #   if(LYNX_EXECUTABLE)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${module_name}/${module_name}.txt
    COMMAND
      # TXT version - just plain text
      ${XMLCATATLOGX} ${XSLTPROC_EXECUTABLE} ${DOCS_XSLTPROC_FLAGS} --xinclude ${TXT_XSL}
      ${module_name}.xml | ${LYNX_EXECUTABLE} ${DOCS_LYNX_FLAGS} -stdin -dump >
      ${CMAKE_CURRENT_BINARY_DIR}/${module_name}/${module_name}.txt
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${module_name}/doc/${module_name}.xml ${TXT_XSL}
            # ${SINGLE_HTML_XSL}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${module_name}/doc
    COMMENT "Generating text documentation with for ${module_name}"
  )

  # Add custom command to copy the README file after the readme target is
  # built. The readme target depends on doc_text so it will regenerate if
  # it's input changed.
  add_custom_command(
    TARGET ${module_name}_readme
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${module_name}/${module_name}.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/${module_name}/README
    COMMENT "Copying README file to source tree for ${module_name}"
  )

  add_custom_command(
    OUTPUT ${DOCS_OUTPUT_DIR}/${module_name}.html
    COMMAND
      # HTML version
      ${XMLCATATLOGX} ${XSLTPROC_EXECUTABLE} ${DOCS_XSLTPROC_FLAGS} --xinclude --stringparam
      base.dir ${DOCS_OUTPUT_DIR} --stringparam root.filename ${module_name} --stringparam
      html.stylesheet ${DOCS_HTML_CSS} --stringparam html.ext ".html" ${SINGLE_HTML_XSL}
      ${module_name}.xml
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${module_name}/doc/${module_name}.xml ${SINGLE_HTML_XSL}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${module_name}/doc
    COMMENT "Generating html documentation for ${module_name}"
  )
  # endif()

  install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/${module_name}/README
    RENAME README.${module_name}
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/modules
    COMPONENT ${group_name}
  )
  # endif()
endfunction()
