cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

# Prevent in-source builds
include(FeatureSummary)
include(PreventInSourceBuilds)

project(rpc-client)

function(use_cxx11)
  if(CMAKE_VERSION VERSION_LESS "3.1")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11" PARENT_SCOPE)
    endif()
  else()
    set(CMAKE_CXX_STANDARD 11 PARENT_SCOPE)
  endif()
endfunction(use_cxx11)

use_cxx11()

# Dependencies
find_package(wiscrpcsvc REQUIRED)

add_executable(rpc-client rpc-client.cpp)
target_include_directories(rpc-client PRIVATE ../include)
target_link_libraries(rpc-client PRIVATE wiscrpcsvc::wiscrpcsvc) # rpcsvc lib
target_link_libraries(rpc-client PRIVATE dl)
install(TARGETS rpc-client DESTINATION bin/)
