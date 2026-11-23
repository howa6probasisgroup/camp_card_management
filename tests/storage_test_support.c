#include "storage_test_support.h"
#include "file_access.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

[[noreturn]] void test_fail(const char *file, const int line,
                            const char *expression) {
  fprintf(stderr, "检查失败：%s:%d: %s\n", file, line, expression);
  exit(EXIT_FAILURE);
}

void test_temp_dir(char *out, const size_t capacity) {
#ifdef _WIN32
  wchar_t base[MAX_PATH], path[MAX_PATH];
  const DWORD n = GetTempPathW(MAX_PATH, base);
  REQUIRE(n && n < MAX_PATH);
  REQUIRE(GetTempFileNameW(base, L"ccm", 0, path));
  REQUIRE(DeleteFileW(path));
  REQUIRE(CreateDirectoryW(path, nullptr));
  REQUIRE(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, -1, out,
                              (int)capacity, nullptr, nullptr));
#else
  REQUIRE(capacity > sizeof("/tmp/camp-card-test-XXXXXX"));
  strcpy(out, "/tmp/camp-card-test-XXXXXX");
  REQUIRE(mkdtemp(out));
#endif
}

void test_remove_directory(const char *path) {
#ifdef _WIN32
  const int count =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
  REQUIRE(count > 0);
  wchar_t *input = malloc((size_t)count * sizeof(*input));
  REQUIRE(input);
  REQUIRE(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, input,
                              count));

  const DWORD needed = GetFullPathNameW(input, 0, nullptr, nullptr);
  REQUIRE(needed > 0);
  wchar_t *absolute = malloc((size_t)needed * sizeof(*absolute));
  REQUIRE(absolute);
  REQUIRE(GetFullPathNameW(input, needed, absolute, nullptr) < needed);
  free(input);
  for (wchar_t *p = absolute; *p; p++)
    if (*p == L'/')
      *p = L'\\';

  const bool unc = absolute[0] == L'\\' && absolute[1] == L'\\';
  const wchar_t *prefix = unc ? L"\\\\?\\UNC\\" : L"\\\\?\\";
  const wchar_t *body = unc ? absolute + 2 : absolute;
  const size_t prefix_length = wcslen(prefix);
  const size_t body_length = wcslen(body);
  wchar_t *extended =
      malloc((prefix_length + body_length + 1) * sizeof(*extended));
  REQUIRE(extended);
  memcpy(extended, prefix, prefix_length * sizeof(*extended));
  memcpy(extended + prefix_length, body, (body_length + 1) * sizeof(*extended));
  free(absolute);

  REQUIRE(RemoveDirectoryW(extended));
  free(extended);
#else
  REQUIRE(rmdir(path) == 0);
#endif
}

static AppResult test_remove_entry(const char *name, const FileAccessKind kind,
                                   void *context) {
  char path[4096];
  REQUIRE(file_access_join(context, name, path, sizeof(path)) == APP_OK);
  if (kind == FILE_ACCESS_DIRECTORY)
    test_remove_tree(path);
  else
    REQUIRE(file_access_remove(path) == APP_OK);
  return APP_OK;
}

void test_remove_tree(const char *path) {
  REQUIRE(file_access_visit_directory(path, test_remove_entry, (void *)path) ==
          APP_OK);
  test_remove_directory(path);
}

void test_path(const char *root, const char *suffix, char *out,
               const size_t capacity) {
  const int n = snprintf(out, capacity, "%s/%s", root, suffix);
  REQUIRE(n > 0 && (size_t)n < capacity);
}
