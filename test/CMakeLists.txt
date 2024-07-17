cmake_minimum_required(VERSION 3.26)
project(Test)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/googletest)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/googletest/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/googlemock/include)

add_executable(test ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_thread_cache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_central_cache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_page_heap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_system_alloc.cpp)
target_link_libraries(test gtest gtest_main)
add_test(NAME test COMMAND test)
