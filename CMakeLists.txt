CMAKE_MINIMUM_REQUIRED(VERSION 3.19.2)

PROJECT(serum-pyth)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -g -ggdb")

set(SOURCES main.cpp)
ADD_EXECUTABLE(serum-pyth main.cpp)

FIND_PACKAGE(OpenSSL REQUIRED)

TARGET_INCLUDE_DIRECTORIES(serum-pyth PRIVATE /home/mass/work/pyth-client/)
TARGET_INCLUDE_DIRECTORIES(serum-pyth PRIVATE /home/mass/work/pyth-client/program/src/)

TARGET_LINK_LIBRARIES(serum-pyth PRIVATE /home/mass/work/pyth-client/build/libpc.a)
TARGET_LINK_LIBRARIES(serum-pyth PRIVATE OpenSSL::SSL OpenSSL::Crypto pthread z)
