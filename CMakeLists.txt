cmake_minimum_required(VERSION 3.5)

project(miniply LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(.)

add_executable(miniply-perf
  miniply.cpp
  miniply.h
  extra/miniply-perf.cpp
)

add_executable(miniply-info
  miniply.cpp
  miniply.h
  extra/miniply-info.cpp
)
