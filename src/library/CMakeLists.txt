cmake_minimum_required(VERSION 3.14)
project(wobc_library)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


enable_testing()

add_library(wobc STATIC algorithm/heap.cpp)
add_executable(test_heap algorithm/test_heap.cpp)
add_executable(test_patricia_tri_tree algorithm/test_patricia_tri_tree.cpp)
target_link_libraries(
  test_heap
  wobc
  GTest::gtest_main
)
target_link_libraries(
  test_patricia_tri_tree
  wobc
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_heap)
gtest_discover_tests(test_patricia_tri_tree)
