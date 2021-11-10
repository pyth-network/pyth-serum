CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

PROJECT(serum-pyth-crank)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -g -ggdb")

set(SOURCES main.cpp)
ADD_EXECUTABLE(serum-pyth-crank main.cpp)

FIND_PACKAGE(OpenSSL REQUIRED)

TARGET_INCLUDE_DIRECTORIES(serum-pyth-crank PRIVATE ../../pyth-client/)
TARGET_INCLUDE_DIRECTORIES(serum-pyth-crank PRIVATE ../../pyth-client/program/src/)

FIND_LIBRARY(L_PC libpc.a ../../pyth-client/build)

TARGET_LINK_LIBRARIES(serum-pyth-crank PRIVATE ${L_PC} OpenSSL::SSL OpenSSL::Crypto pthread z zstd)

ENABLE_TESTING()
