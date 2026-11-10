// CLI 输入边界的可复现测试。通过宏替换 getchar/ferror 注入可控输入序列，
// 直接包含 src/cli.c 以访问内部读行逻辑，验证：
//   - 空行被忽略，正常读取后续输入；
//   - CRLF 行尾被清理，EOF 前无换行的最后一行仍按一行读取；
//   - EOF 正常结束程序（cli_run 返回 APP_OK）；
//   - 输入流故障返回 APP_ERR_IO；
//   - 超长行排空当前行，残留字符不会被当作下一次输入；
//   - 重复输入可复现地稳定读取。
//
// 本测试只验证 CLI 自身的输入处理；cli_run 调用的业务接口由
// tests/cli_stub_services.c 提供固定结果的桩，不依赖真实服务层与存储。

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 用测试桩替换 getchar/ferror，须在包含 stdio.h 之前声明，且签名与
// 被替换函数一致。cli.c 通过 getchar()/ferror() 读取输入。
static int cli_test_getchar();
static int cli_test_ferror(const FILE *stream);

#define getchar cli_test_getchar
#define ferror cli_test_ferror

// 包含实现以访问 static 的读行逻辑与 cli_run。
#include "../src/cli.c"

#undef getchar
#undef ferror

// 测试桩状态：由当前用例填充的字符序列。
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

// 直接调用内部读行函数，检查返回与行内容。
static bool read_line(CliLine *line) { return cli_read_line(line); }

static void test_eof_returns_false() {
  // 空输入立即 EOF：读行返回 false，标记 eof。
  reset_input("");
  CliLine line;
  CHECK(read_line(&line) == false);
  CHECK(line.eof == true);
  CHECK(line.io_error == false);
  CHECK(line.length == 0);
}

static void test_short_line() {
  // 普通行：去除末尾换行符，返回 true。
  reset_input("hello\n");
  CliLine line;
  CHECK(read_line(&line) == true);
  CHECK(line.eof == false);
  CHECK(line.length == 5);
  CHECK(strncmp(line.buffer, "hello", 5) == 0);
  CHECK(line.buffer[5] == '\0');
}

static void test_empty_line() {
  // 空行：读取到换行，返回 true，length 为 0。
  reset_input("\n");
  CliLine line;
  CHECK(read_line(&line) == true);
  CHECK(line.length == 0);
  CHECK(line.buffer[0] == '\0');
}

static void test_crlf_line() {
  // Windows 文本输入的 CRLF 行尾：去掉回车和换行，不把 '\r' 留给调用方。
  reset_input("hello\r\n");
  CliLine line;
  CHECK(read_line(&line) == true);
  CHECK(line.length == 5);
  CHECK(strcmp(line.buffer, "hello") == 0);
}

static void test_eof_with_partial_line() {
  // EOF 前有无换行的内容时按一行处理；下一次读取才返回 EOF。
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
  // EOF 同时伴随流故障：标记 io_error。
  reset_input("");
  g_force_error = true;
  CliLine line;
  CHECK(read_line(&line) == false);
  CHECK(line.eof == true);
  CHECK(line.io_error == true);
}

static void test_long_line_is_drained() {
  // 超长行（超出 CLI_LINE_CAPACITY）会被截断存储，剩余字符排空，不当作
  // 下一次输入。构造：超长行 + 换行 + 后续有效行。
  constexpr size_t long_len = CLI_LINE_CAPACITY; // 填满缓冲
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
  // 超长行：因容量提前停止，读取返回 true（缓冲已满）并标记超长，
  // 随后排空当前行。
  CHECK(read_line(&first) == true);
  CHECK(first.length + 1 == sizeof first.buffer);
  CHECK(first.too_long == true);

  // 剩余字符已排空，下一次读取应得到 "ok" 行而不是超长行的残留。
  CliLine second;
  CHECK(read_line(&second) == true);
  CHECK(second.length == 2);
  CHECK(strncmp(second.buffer, "ok", 2) == 0);
  CHECK(second.buffer[2] == '\0');

  free(input);
}

static void test_repeated_input_remains_stable() {
  // 重复输入可复现地稳定读取：同一序列多次读取结果一致。
  const char *input = "a\nbb\nccc\n";
  CliLine a, b, c;
  reset_input(input);
  CHECK(read_line(&a) == true && a.length == 1);
  CHECK(read_line(&b) == true && b.length == 2);
  CHECK(read_line(&c) == true && c.length == 3);

  // 再次用同一序列，读取结果应一致。
  reset_input(input);
  CliLine a2, b2, c2;
  CHECK(read_line(&a2) == true && a2.length == 1);
  CHECK(read_line(&b2) == true && b2.length == 2);
  CHECK(read_line(&c2) == true && c2.length == 3);
}

static void test_cli_run_eof_returns_ok() {
  // cli_run 在 EOF 时正常退出返回 APP_OK。
  reset_input("");
  CHECK(cli_run() == APP_OK);
}

static void test_cli_run_io_error_returns_io() {
  // cli_run 在流故障时返回 APP_ERR_IO。
  reset_input("");
  g_force_error = true;
  CHECK(cli_run() == APP_ERR_IO);
}

static void test_cli_run_ignores_empty_lines_and_runs_to_eof() {
  // 混合空行与普通行，最终读到 EOF 返回 APP_OK。
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
