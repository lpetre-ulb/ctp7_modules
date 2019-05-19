# Try to find the wiscrpcsvc client library

find_library(wiscrpcsvc_LIBRARY
  NAMES wiscrpcsvc
  PATHS /opt/wiscrpcsvc/
  PATH_SUFFIXES lib/
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(wiscrpcsvc DEFAULT_MSG
  wiscrpcsvc_LIBRARY
)

mark_as_advanced(wiscrpcsvc_LIBRARY)

if(WISCRPCSVC_FOUND)
  set(wiscrpcsvc_LIBRARIES ${wiscrpcsvc_LIBRARY})

  if(NOT TARGET wiscrpcsvc::wiscrpcsvc)
    add_library(wiscrpcsvc::wiscrpcsvc SHARED IMPORTED)
    set_target_properties(wiscrpcsvc::wiscrpcsvc PROPERTIES
      IMPORTED_LOCATION "${wiscrpcsvc_LIBRARY}"
      IMPORTED_NO_SONAME TRUE
    )
  endif()
endif()
