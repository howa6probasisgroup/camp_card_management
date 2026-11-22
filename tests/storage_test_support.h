#ifndef CAMP_CARD_STORAGE_TEST_SUPPORT_H
#define CAMP_CARD_STORAGE_TEST_SUPPORT_H
#ifndef _WIN32
// POSIX 规定的功能开关名称，不能重命名。
// NOLINTNEXTLINE(bugprone-reserved-identifier)
#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
// macOS 规定的功能开关名称，不能重命名。
// NOLINTNEXTLINE(bugprone-reserved-identifier)
#define _DARWIN_C_SOURCE
#endif
#endif

#include <stddef.h>

[[noreturn]] void test_fail(const char *file, int line, const char *expression);

/* 独立临时目录；失败立即停止，避免继续操作未初始化的测试路径。 */
#define REQUIRE(expr)                                                          \
  do {                                                                         \
    if (!(expr))                                                               \
      test_fail(__FILE__, __LINE__, #expr);                                    \
  } while (0)

void test_temp_dir(char *out, size_t capacity);
void test_remove_directory(const char *path);
void test_remove_tree(const char *path);
void test_path(const char *root, const char *suffix, char *out,
               size_t capacity);
#endif
