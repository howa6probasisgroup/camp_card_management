cmake_minimum_required(VERSION 4.3)
project(camp_card_management C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(yyjson CONFIG REQUIRED)
find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)

# 除入口点外的后端源码集中在静态库中，供可执行目标与测试目标共用。
add_library(camp_card_core STATIC
    src/data_structures.c
)
target_include_directories(camp_card_core PUBLIC include)
target_link_libraries(camp_card_core PUBLIC yyjson::yyjson OpenSSL::Crypto)

add_executable(camp_card_management main.c)
target_link_libraries(camp_card_management PRIVATE camp_card_core)

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(camp_card_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(camp_card_management PRIVATE -Wall -Wextra -Wpedantic)
endif ()

include(CTest)

if (BUILD_TESTING)
    add_executable(test_data_structures tests/test_data_structures.c)
    target_link_libraries(test_data_structures PRIVATE camp_card_core)
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(test_data_structures PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
    add_test(NAME data_structures COMMAND test_data_structures)
endif ()
