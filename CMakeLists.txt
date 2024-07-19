cmake_minimum_required(VERSION 3.20)
project(MemoryPool)

set(CMAKE_CXX_STANDARD 17)

# enable ctest/make test
enable_testing()

include_directories(src/singleton
        src/list
        src/cache
        src/common
        src/map
        third)

add_executable(MemoryPool src/main.cpp)

target_link_libraries(MemoryPool pthread)

add_subdirectory(test)

