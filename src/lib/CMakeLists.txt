set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Each library is adding it's sources into the kamailio binary.
file(
  GLOB children
  RELATIVE ${LIBS_DIR}
  LIST_DIRECTORIES true
  *
)

foreach(child ${children})
  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${child}
     AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt
  )
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${child})

  endif()
endforeach()
