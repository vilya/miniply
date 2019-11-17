cmake_minimum_required(VERSION 3.5)

project(miniply LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(3rdparty/happly)

add_executable(miniply
  main.cpp
  miniply.cpp
  miniply.h

  3rdparty/happly/happly.h
)
