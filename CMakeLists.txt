cmake_minimum_required(VERSION 2.8)
project(WPELauncher)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(WPEWebKit REQUIRED)

file(WRITE ${CMAKE_BINARY_DIR}/test_atomic.cpp
     "#include <atomic>\n"
     "int main() { std::atomic<int64_t> i(0); i++; return 0; }\n")
try_compile(ATOMIC_BUILD_SUCCEEDED ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/test_atomic.cpp)
if (NOT ATOMIC_BUILD_SUCCEEDED)
    list(APPEND WPE_WEBKIT_LIBRARIES atomic)
endif ()
file(REMOVE ${CMAKE_BINARY_DIR}/test_atomic.cpp)

add_subdirectory(launcher)
