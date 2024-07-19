cmake_minimum_required(VERSION 3.20)
project(Test)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/googletest)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/googletest/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/googlemock/include)

add_executable(test_thread_cache ${CMAKE_CURRENT_SOURCE_DIR}/test_thread_cache.cpp)
target_link_libraries(test_thread_cache gtest gtest_main)

add_executable(test_central_cache ${CMAKE_CURRENT_SOURCE_DIR}/test_central_cache.cpp)
target_link_libraries(test_central_cache gtest gtest_main)

add_executable(test_page_heap ${CMAKE_CURRENT_SOURCE_DIR}/test_page_heap.cpp)
target_link_libraries(test_page_heap gtest gtest_main)

add_executable(test_system_alloc ${CMAKE_CURRENT_SOURCE_DIR}/test_system_alloc.cpp)
target_link_libraries(test_system_alloc gtest gtest_main)

add_test(NAME test_thread_cache COMMAND test_thread_cache)

add_test(NAME test_central_cache COMMAND test_central_cache)

add_test(NAME test_page_heap COMMAND test_page_heap)

add_test(NAME test_system_alloc COMMAND test_system_alloc)
