# Try to find libmemsvc

find_library(Libmemsvc_LIBRARY
  NAMES memsvc
)
find_path(Libmemsvc_INCLUDE_DIR
  NAMES libmemsvc.h
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libmemsvc DEFAULT_MSG
  Libmemsvc_LIBRARY Libmemsvc_INCLUDE_DIR
)

mark_as_advanced(Libmemsvc_INCLUDE_DIR Libmemsvc_LIBRARY)

if(LIBMEMSVC_FOUND)
  set(Libmemsvc_LIBRARIES ${Libmemsvc_LIBRARY})
  set(Libmemsvc_INCLUDE_DIRS ${Libmemsvc_INCLUDE_DIR})

  if(NOT TARGET Libmemsvc::Libmemsvc)
    add_library(Libmemsvc::Libmemsvc SHARED IMPORTED)
    set_target_properties(Libmemsvc::Libmemsvc PROPERTIES
      IMPORTED_LOCATION "${Libmemsvc_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${Libmemsvc_INCLUDE_DIR}"
      IMPORTED_NO_SONAME TRUE
    )
  endif()
endif()
