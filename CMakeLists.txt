cmake_minimum_required(VERSION 4.0)

project(cvtree)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/libomp")
find_package(OpenMP REQUIRED)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(cvtree improved.cpp)
target_link_libraries(cvtree PRIVATE OpenMP::OpenMP_CXX)