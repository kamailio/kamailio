file(
  GLOB children
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
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
