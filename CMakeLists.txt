cmake_minimum_required(VERSION 3.20)
project(osshell CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow)

add_library(osshell INTERFACE)
target_include_directories(osshell INTERFACE include)

add_executable(osh src/osh.cpp)
target_link_libraries(osh PRIVATE osshell)

add_executable(schedbench src/schedbench.cpp)
target_link_libraries(schedbench PRIVATE osshell)

enable_testing()
add_subdirectory(tests)
