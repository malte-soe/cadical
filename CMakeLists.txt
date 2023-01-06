cmake_minimum_required(VERSION 3.0)
project(Cadical C CXX)

set(VERSION "0.0")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
add_subdirectory(src)
