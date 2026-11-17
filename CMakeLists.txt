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

add_library(camp_card_services STATIC
    src/services/account_service.c
    src/services/auth_service.c
    src/services/card_service.c
    src/services/service_helpers.c
)
target_link_libraries(camp_card_services PUBLIC camp_card_core)

add_executable(camp_card_management main.c)
target_link_libraries(camp_card_management PRIVATE camp_card_core)

# CLI 库独立编译，便于分别检查菜单代码和业务服务。静态库允许保留对
# 真实存储接口的未解析引用，待存储实现接入后再链接正式入口。
add_library(camp_card_cli STATIC
    src/cli.c
)
target_link_libraries(camp_card_cli PUBLIC camp_card_core)

# 真实存储实现尚未进入本仓库时，入口仍然编译，但明确提示无法启动，
# 不伪造业务成功。存储实现接入 camp_card_core 后开启该选项即可链接正式入口。
option(CAMP_CARD_BACKEND_READY "真实存储实现已接入，可链接正式入口" OFF)
if (CAMP_CARD_BACKEND_READY)
    target_compile_definitions(camp_card_management PRIVATE CAMP_CARD_BACKEND_READY)
    target_link_libraries(
        camp_card_management PRIVATE camp_card_cli camp_card_services
    )
else ()
    message(STATUS "真实存储实现未接入：正式入口只提示不可用")
endif ()

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(camp_card_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(camp_card_services PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(camp_card_management PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(camp_card_cli PRIVATE -Wall -Wextra -Wpedantic)
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

    # CLI 测试。parse_amount 链接正式 cli.c 与只返回固定结果的服务桩；
    # input/menu 测试包含 cli.c 以替换输入函数，因此不重复链接 cli 库。
    add_executable(test_cli_parse_amount
        tests/test_cli_parse_amount.c
        tests/cli_stub_services.c
    )
    target_link_libraries(test_cli_parse_amount PRIVATE camp_card_cli)
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(test_cli_parse_amount PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
    add_test(NAME cli_parse_amount COMMAND test_cli_parse_amount)

    add_executable(test_cli_input tests/test_cli_input.c tests/cli_stub_services.c)
    target_link_libraries(test_cli_input PRIVATE camp_card_core)
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(test_cli_input PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
    add_test(NAME cli_input COMMAND test_cli_input)

    add_executable(test_cli_menu tests/test_cli_menu.c)
    target_link_libraries(test_cli_menu PRIVATE camp_card_core)
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(test_cli_menu PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
    add_test(NAME cli_menu COMMAND test_cli_menu)

    add_executable(
        test_auth_service
        tests/test_auth_service.c
        tests/service_test_support.c
    )
    target_link_libraries(test_auth_service PRIVATE camp_card_services)
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            test_auth_service PRIVATE -Wall -Wextra -Wpedantic
        )
    endif ()
    add_test(NAME auth_service COMMAND test_auth_service)

    add_executable(
        test_account_service
        tests/test_account_service.c
        tests/service_test_support.c
    )
    target_link_libraries(test_account_service PRIVATE camp_card_services)
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            test_account_service PRIVATE -Wall -Wextra -Wpedantic
        )
    endif ()
    add_test(NAME account_service COMMAND test_account_service)

    add_executable(
        test_card_service
        tests/test_card_service.c
        tests/service_test_support.c
    )
    target_link_libraries(test_card_service PRIVATE camp_card_services)
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            test_card_service PRIVATE -Wall -Wextra -Wpedantic
        )
    endif ()
    add_test(NAME card_service COMMAND test_card_service)
endif ()
