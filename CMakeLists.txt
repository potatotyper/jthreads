cmake_minimum_required(VERSION 3.15)
project(jthreads VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_library(jthreads
  src/jthreads/jthread.cpp
  src/jthreads/threadpool.cpp
  src/jthreads/tracer.cpp
)

target_include_directories(jthreads PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(jthreads PUBLIC Threads::Threads)

enable_testing()

add_executable(sample examples/sample.cpp)
target_link_libraries(sample PRIVATE jthreads)
