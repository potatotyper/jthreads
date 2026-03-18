cmake_minimum_required(VERSION 3.15)
project(jthreads VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(jthreads
  src/jthreads/threadpool.cpp
  src/jthreads/tracer.cpp
)

target_include_directories(jthreads PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

enable_testing()

add_executable(basicprogram1 examples/basicprogram1.cpp)
target_link_libraries(basicprogram1 PRIVATE jthreads)
