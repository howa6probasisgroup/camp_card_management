/**
 * @file test_cli_input.c
 * @brief 验证 CLI 的行输入边界。
 * @details 覆盖 EOF、CRLF、流故障、超长行排空和重复读取；替换
 *          `getchar`/`ferror` 注入输入，业务接口由固定结果的测试桩提供。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 声明须位于包含 cli.c 之前，使其内部的 getchar/ferror 调用使用测试桩。
static int cli_test_getchar();
static int cli_test_ferror(const FILE *stream);

#define getchar cli_test_getchar
#define ferror cli_test_ferror

#include "../src/cli.c"

#undef getchar
#undef ferror

static const char *g_input;
static size_t g_position;
static bool g_force_eof;
static bool g_force_error;

static void reset_input(const char *text) {
  g_input = text;
  g_position = 0;
  g_force_eof = false;
  g_force_error = false;
}

static int cli_test_getchar() {
  if (g_force_eof || g_input == nullptr || g_input[g_position] == '\0') {
    return EOF;
  }
  return (unsigned char)g_input[g_position++];
}

static int cli_test_ferror(const FILE *stream) {
  (void)stream;
  return g_force_error ? 1 : 0;
}

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

static bool read_line(CliLine *line) { return cli_read_line(line); }

static void test_eof_returns_false() {
  reset_input("");
  CliLine line;
  CHECK(read_line(&line) == false);
  CHECK(line.eof == true);
  CHECK(line.io_error == false);
  CHECK(line.length == 0);
}

static void test_short_line() {
  reset_input("hello\n");
  CliLine line;
  CHECK(read_line(&line) == true);
  CHECK(line.eof == false);
  CHECK(line.length == 5);
  CHECK(strncmp(line.buffer, "hello", 5) == 0);
  CHECK(line.buffer[5] == '\0');
}

static void test_empty_line() {
  reset_input("\n");
  CliLine line;
  CHECK(read_line(&line) == true);
  CHECK(line.length == 0);
  CHECK(line.buffer[0] == '\0');
}

static void test_crlf_line() {
  reset_input("hello\r\n");
  CliLine line;
  CHECK(read_line(&line) == true);
  CHECK(line.length == 5);
  CHECK(strcmp(line.buffer, "hello") == 0);
}

static void test_eof_with_partial_line() {
  reset_input("last");
  CliLine line;
  CHECK(read_line(&line) == true);
  CHECK(line.length == 4);
  CHECK(strcmp(line.buffer, "last") == 0);
  CHECK(line.eof == true);
  CHECK(read_line(&line) == false);
  CHECK(line.eof == true);
}

static void test_io_error_marks_failure() {
  reset_input("");
  g_force_error = true;
  CliLine line;
  CHECK(read_line(&line) == false);
  CHECK(line.eof == true);
  CHECK(line.io_error == true);
}

static void test_long_line_is_drained() {
  // 构造超长行、换行和后续有效行，验证残留内容不会串到下一次读取。
  constexpr size_t long_len = CLI_LINE_CAPACITY;
  char *input = malloc(long_len + 16);
  if (input == nullptr) {
    CHECK(false);
    return;
  }
  memset(input, 'x', long_len);
  input[long_len] = '\n';
  input[long_len + 1] = 'o';
  input[long_len + 2] = 'k';
  input[long_len + 3] = '\n';
  input[long_len + 4] = '\0';

  reset_input(input);
  CliLine first;
  CHECK(read_line(&first) == true);
  CHECK(first.length + 1 == sizeof first.buffer);
  CHECK(first.too_long == true);

  CliLine second;
  CHECK(read_line(&second) == true);
  CHECK(second.length == 2);
  CHECK(strncmp(second.buffer, "ok", 2) == 0);
  CHECK(second.buffer[2] == '\0');

  free(input);
}

static void test_repeated_input_remains_stable() {
  const char *input = "a\nbb\nccc\n";
  CliLine a, b, c;
  reset_input(input);
  CHECK(read_line(&a) == true && a.length == 1);
  CHECK(read_line(&b) == true && b.length == 2);
  CHECK(read_line(&c) == true && c.length == 3);

  reset_input(input);
  CliLine a2, b2, c2;
  CHECK(read_line(&a2) == true && a2.length == 1);
  CHECK(read_line(&b2) == true && b2.length == 2);
  CHECK(read_line(&c2) == true && c2.length == 3);
}

static void test_cli_run_eof_returns_ok() {
  reset_input("");
  CHECK(cli_run() == APP_OK);
}

static void test_cli_run_io_error_returns_io() {
  reset_input("");
  g_force_error = true;
  CHECK(cli_run() == APP_ERR_IO);
}

static void test_cli_run_ignores_empty_lines_and_runs_to_eof() {
  reset_input("\nfoo\n\nbar\n\n");
  CHECK(cli_run() == APP_OK);
}

int main() {
  test_eof_returns_false();
  test_short_line();
  test_empty_line();
  test_crlf_line();
  test_eof_with_partial_line();
  test_io_error_marks_failure();
  test_long_line_is_drained();
  test_repeated_input_remains_stable();
  test_cli_run_eof_returns_ok();
  test_cli_run_io_error_returns_io();
  test_cli_run_ignores_empty_lines_and_runs_to_eof();

  if (g_failures == 0) {
    printf("test_cli_input: %zu checks passed\n", g_checks);
    return EXIT_SUCCESS;
  }
  fprintf(stderr, "test_cli_input: %zu of %zu checks failed\n", g_failures,
          g_checks);
  return EXIT_FAILURE;
}
