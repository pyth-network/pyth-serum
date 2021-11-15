CMAKE_MINIMUM_REQUIRED( VERSION 3.13 )

PROJECT( serum-pyth-crank )

if( NOT PC )
  set( PC ../../pyth-client )
endif()

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_VERBOSE_MAKEFILE ON )

ADD_COMPILE_OPTIONS(
  -g
  -ggdb
  -m64
  -Wall
  -Werror
  -Wextra
  -Wsign-conversion
)

ADD_EXECUTABLE(
  serum-pyth-crank
  main.cpp
)

TARGET_INCLUDE_DIRECTORIES(
  serum-pyth-crank
  PRIVATE
  ${PC}
  ${PC}/program/src
)

FIND_LIBRARY(
  L_PC
  libpc.a
  ${PC}/build
)

TARGET_LINK_LIBRARIES(
  serum-pyth-crank
  PRIVATE
  ${L_PC}
  ssl
  crypto
  pthread
  z
  zstd
)

ENABLE_TESTING()
