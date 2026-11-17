#ifndef CAMP_CARD_STORAGE_TEST_SUPPORT_H
#define CAMP_CARD_STORAGE_TEST_SUPPORT_H
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#endif
#include "file_access.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* 独立临时目录；失败立即停止，避免继续操作未初始化的测试路径。 */
#define REQUIRE(expr)                                                          \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "检查失败：%s:%d: %s\n", __FILE__, __LINE__, #expr);     \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

static inline void test_temp_dir(char *out, size_t capacity) {
#ifdef _WIN32
  wchar_t base[MAX_PATH], path[MAX_PATH];
  DWORD n = GetTempPathW(MAX_PATH, base);
  REQUIRE(n && n < MAX_PATH);
  REQUIRE(GetTempFileNameW(base, L"ccm", 0, path));
  REQUIRE(DeleteFileW(path));
  REQUIRE(CreateDirectoryW(path, NULL));
  REQUIRE(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, -1, out,
                              (int)capacity, NULL, NULL));
#else
  REQUIRE(capacity > sizeof("/tmp/camp-card-test-XXXXXX"));
  strcpy(out, "/tmp/camp-card-test-XXXXXX");
  REQUIRE(mkdtemp(out));
#endif
}
static inline void test_remove_directory(const char *path) {
#ifdef _WIN32
  wchar_t wide[4096];
  REQUIRE(
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, 4096));
  REQUIRE(RemoveDirectoryW(wide));
#else
  REQUIRE(rmdir(path) == 0);
#endif
}
static inline void test_remove_tree(const char *path);
static inline AppResult test_remove_entry(const char *name, FileAccessKind kind,
                                          void *context) {
  char path[4096];
  REQUIRE(file_access_join(context, name, path, sizeof(path)) == APP_OK);
  if (kind == FILE_ACCESS_DIRECTORY)
    test_remove_tree(path);
  else
    REQUIRE(file_access_remove(path) == APP_OK);
  return APP_OK;
}
static inline void test_remove_tree(const char *path) {
  REQUIRE(file_access_visit_directory(path, test_remove_entry, (void *)path) ==
          APP_OK);
  test_remove_directory(path);
}
static inline void test_path(const char *root, const char *suffix, char *out,
                             size_t capacity) {
  int n = snprintf(out, capacity, "%s/%s", root, suffix);
  REQUIRE(n > 0 && (size_t)n < capacity);
}
#endif
