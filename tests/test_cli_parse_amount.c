#include "cli.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// CLI 金额解析的独立测试。覆盖整数、一位与两位小数、零、非法字符、
// 格式错误和范围边界；验证失败不改变输出，全程不引入浮点数。
//
// 解析契约（以 cli.h 为准）：
//   只接受非负十进制整数，或最多两位小数的十进制数；
//   不允许正负号、指数、逗号、空白或多余字符；
//   至少一位整数；"." 之后须有 1..2 位数字；接受 0；
//   溢出返回 INVALID_ARGUMENT；失败不改变 out_cents。

static size_t g_checks;
static size_t g_failures;

static void check(const bool condition, const char *expression,
                  const int line) {
  g_checks++;
  if (!condition) {
    g_failures++;
    fprintf(stderr, "FAIL line %d: %s\n", line, expression);
  }
}

#define CHECK(condition) check((condition), #condition, __LINE__)

// 断言解析结果与期望分值一致，且失败时不改变输出。
static void expect_parse(const char *text, const AppResult expected,
                         const int64_t expected_cents) {
  int64_t cents = -1234567890; // 哨兵值，用于检测失败时输出被修改
  const AppResult result = cli_parse_amount(text, &cents);
  CHECK(result == expected);
  if (result == APP_OK) {
    if (cents != expected_cents) {
      fprintf(stderr, "FAIL parse \"%s\": expected %lld, got %lld\n", text,
              (long long)expected_cents, (long long)cents);
    }
    CHECK(cents == expected_cents);
  } else {
    // 失败不得修改 out_cents。
    CHECK(cents == -1234567890);
  }
}

#define EXPECT_PARSE(text, expected, cents)                                    \
  expect_parse((text), (expected), (cents))

static void test_null_arguments() {
  // 空指针参数返回 INVALID_ARGUMENT。
  CHECK(cli_parse_amount(nullptr, nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(cli_parse_amount(nullptr, &(int64_t){0}) == APP_ERR_INVALID_ARGUMENT);
  {
    int64_t cents = 0;
    CHECK(cli_parse_amount("100", nullptr) == APP_ERR_INVALID_ARGUMENT);
    CHECK(cents == 0); // 输出保持不变
  }
}

static void test_empty_and_non_numeric() {
  EXPECT_PARSE("", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("abc", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("x123", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("1a", APP_ERR_INVALID_ARGUMENT, 0);
}

static void test_signs_and_exponents() {
  // 正负号、指数均非法。
  EXPECT_PARSE("+100", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("-100", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("-0", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("1e3", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("1E3", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("1e+3", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("1e-3", APP_ERR_INVALID_ARGUMENT, 0);
}

static void test_whitespace_and_separators() {
  // 前后空白、内部空格、逗号均非法。
  EXPECT_PARSE(" 100", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("100 ", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("1 00", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("\t100", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("100\n", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("1,000", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("100,", APP_ERR_INVALID_ARGUMENT, 0);
}

static void test_integer_amounts() {
  // 非负十进制整数：换算为分。
  EXPECT_PARSE("0", APP_OK, 0);
  EXPECT_PARSE("00", APP_OK, 0);
  EXPECT_PARSE("1", APP_OK, 100);
  EXPECT_PARSE("9", APP_OK, 900);
  EXPECT_PARSE("10", APP_OK, 1000);
  EXPECT_PARSE("100", APP_OK, 10000);
  EXPECT_PARSE("999", APP_OK, 99900);
  EXPECT_PARSE("123456789", APP_OK, 12345678900LL);
}

static void test_decimal_amounts() {
  // 一位与两位小数：换算为分。
  EXPECT_PARSE("0.1", APP_OK, 10);
  EXPECT_PARSE("0.12", APP_OK, 12);
  EXPECT_PARSE("0.0", APP_OK, 0);
  EXPECT_PARSE("1.0", APP_OK, 100);
  EXPECT_PARSE("1.2", APP_OK, 120);
  EXPECT_PARSE("1.23", APP_OK, 123);
  EXPECT_PARSE("12.34", APP_OK, 1234);
  EXPECT_PARSE("100.01", APP_OK, 10001);
  EXPECT_PARSE("100.5", APP_OK, 10050);
}

static void test_decimal_format_errors() {
  // "." 后必须紧跟数字；最多两位小数。
  EXPECT_PARSE(".5", APP_ERR_INVALID_ARGUMENT, 0);    // 缺整数位
  EXPECT_PARSE("1.", APP_ERR_INVALID_ARGUMENT, 0);    // 缺小数位
  EXPECT_PARSE("1.234", APP_ERR_INVALID_ARGUMENT, 0); // 三位小数
  EXPECT_PARSE("1.23.4", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("1.a", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("1.2a", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("1..2", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE(".", APP_ERR_INVALID_ARGUMENT, 0);
}

static void test_range_boundaries() {
  // 边界值：INT64_MAX 的分值上限。整数输入按元换算为分，因此可接受的
  // 最大整数是 92233720368547758 元（即 9223372036854775800 分）。
  EXPECT_PARSE("92233720368547758.07", APP_OK, INT64_MAX);
  EXPECT_PARSE("92233720368547758", APP_OK, 9223372036854775800LL);
  // 整数按元换算后超出 int64 上界：溢出。
  EXPECT_PARSE("92233720368547759", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("9223372036854775807", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("9223372036854775808", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("92233720368547758.08", APP_ERR_INVALID_ARGUMENT, 0);
  EXPECT_PARSE("92233720368547758070", APP_ERR_INVALID_ARGUMENT, 0);
  // 多位数输入溢出被拒绝。
  EXPECT_PARSE("99999999999999999999999999", APP_ERR_INVALID_ARGUMENT, 0);
}

static void test_output_not_modified_on_failure() {
  // 失败时 out_cents 必须保持不变。
  int64_t cents = 42;
  CHECK(cli_parse_amount("12.345", &cents) == APP_ERR_INVALID_ARGUMENT);
  CHECK(cents == 42);
  CHECK(cli_parse_amount("", &cents) == APP_ERR_INVALID_ARGUMENT);
  CHECK(cents == 42);
  CHECK(cli_parse_amount("99999999999999999999999999", &cents) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(cents == 42);
  // 成功后输出更新为解析结果。
  CHECK(cli_parse_amount("5.5", &cents) == APP_OK);
  CHECK(cents == 550);
}

int main() {
  test_null_arguments();
  test_empty_and_non_numeric();
  test_signs_and_exponents();
  test_whitespace_and_separators();
  test_integer_amounts();
  test_decimal_amounts();
  test_decimal_format_errors();
  test_range_boundaries();
  test_output_not_modified_on_failure();

  if (g_failures == 0) {
    printf("test_cli_parse_amount: %zu checks passed\n", g_checks);
    return EXIT_SUCCESS;
  }
  fprintf(stderr, "test_cli_parse_amount: %zu of %zu checks failed\n",
          g_failures, g_checks);
  return EXIT_FAILURE;
}
