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
    src/password.c
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

    add_executable(test_password tests/test_password.c)
    target_link_libraries(test_password PRIVATE camp_card_core)
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(test_password PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
    add_test(NAME password COMMAND test_password)

    # 密码实现另编译一份用于故障测试，仅在该测试目标中替换 OpenSSL 调用；
    # 不链接 camp_card_core，正式库中的 OpenSSL 调用保持不变。

    add_executable(test_password_failures tests/test_password_failures.c src/password.c)
    target_include_directories(test_password_failures PRIVATE include)
    target_link_libraries(test_password_failures PRIVATE OpenSSL::Crypto)
    target_compile_definitions(test_password_failures PRIVATE
        RAND_bytes=password_test_rand_bytes
        PKCS5_PBKDF2_HMAC=password_test_pbkdf2
    )
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(test_password_failures PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
    add_test(NAME password_failures COMMAND test_password_failures)
endif ()
