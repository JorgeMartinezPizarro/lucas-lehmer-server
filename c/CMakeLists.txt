cmake_minimum_required(VERSION 3.10)
project(main)

set(CMAKE_CXX_STANDARD 14)

find_package(Threads REQUIRED)

# Manually specify GMP include and library directories
include_directories(/usr/include)
link_directories(/usr/lib/x86_64-linux-gnu)

find_package(Crow CONFIG REQUIRED)

add_executable(main main.cpp)

target_link_libraries(main gmp Threads::Threads Crow::Crow)

