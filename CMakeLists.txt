cmake_minimum_required(VERSION 4.3)
project(camp_card_management C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(yyjson CONFIG REQUIRED)
find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)

add_executable(camp_card_management main.c)
target_include_directories(camp_card_management PRIVATE include)
target_link_libraries(camp_card_management PRIVATE yyjson::yyjson OpenSSL::Crypto)

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(camp_card_management PRIVATE -Wall -Wextra -Wpedantic)
endif ()
