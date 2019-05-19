cmake_minimum_required(VERSION 3.13)
project(ctp7_modules)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# This function creates RPC modules
include(CMakeParseArguments)
function(add_module NAME)
  cmake_parse_arguments(ARGS "" "" "DEPS;LIBS" ${ARGN})
    
  add_library(${NAME} SHARED src/${NAME})
  target_link_libraries(${NAME} PRIVATE memhub ${ARGS_DEPS} ${ARGS_LIBS})

  # Remove the `lib` prefix
  set_target_properties(${NAME} PROPERTIES PREFIX "")

  install(TARGETS ${NAME} LIBRARY DESTINATION /mnt/persistent/rpcmodules)
  set_target_properties(${NAME} PROPERTIES INSTALL_RPATH /mnt/persistent/rpcmodules)
endfunction()

# A few definitions
set(CMAKE_CXX_STANDARD 11)
add_definitions(-DGEM_VARIANT=ge11)

# All headers are in `include/`
include_directories("${CMAKE_SOURCE_DIR}/include")

# Add memhub
find_package(Libmemsvc REQUIRED)

add_library(memhub SHARED src/memhub.cpp)
target_link_libraries(memhub PUBLIC Libmemsvc::Libmemsvc)
install(TARGETS memhub
  LIBRARY DESTINATION /mnt/persistent/rpcmodules)

# Add the modules
add_module(memory)
