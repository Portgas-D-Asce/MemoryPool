cmake_minimum_required(VERSION 3.26)
project(MemoryPool)

set(CMAKE_CXX_STANDARD 17)

# enable ctest
# enable_testing()

include_directories(src/singleton
        src/list
        src/cache
        src/common
        src/map
        include)

add_executable(MemoryPool src/main.cpp)

add_subdirectory(test)
