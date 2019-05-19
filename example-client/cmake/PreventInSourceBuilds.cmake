# Heavily inspired by https://stackoverflow.com/a/22636874

function(PreventInSourceBuilds)
  get_filename_component(SRCDIR "${CMAKE_SOURCE_DIR}" REALPATH)
  get_filename_component(BINDIR "${CMAKE_BINARY_DIR}" REALPATH)

  if("${SRCDIR}" STREQUAL "${BINDIR}")
    message("In-source build are not allowed.")
    message("Typically, you can run cmake from a subdirectory called build.")
    message(FATAL_ERROR "")
  endif()
endfunction()

PreventInSourceBuilds()
