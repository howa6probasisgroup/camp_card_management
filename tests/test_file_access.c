#include "storage_test_support.h"
#ifndef _WIN32
#include <sys/stat.h>
#endif
static AppResult count_entry(const char *name, FileAccessKind kind,
                             void *context) {
  REQUIRE(strcmp(name, ".") != 0 && strcmp(name, "..") != 0);
  REQUIRE(!strchr(name, '/'));
  REQUIRE(kind == FILE_ACCESS_REGULAR || kind == FILE_ACCESS_DIRECTORY ||
          kind == FILE_ACCESS_OTHER);
  (*(size_t *)context)++;
  return APP_OK;
}
static AppResult stop_entry(const char *name, FileAccessKind kind,
                            void *context) {
  (void)name;
  (void)kind;
  (*(size_t *)context)++;
  return APP_ERR_INVALID_DATA;
}
int main() {
  char root[4096], path[4096], nested[4096];
  test_temp_dir(root, sizeof(root));
  test_path(root, "中文目录/a/b", nested, sizeof(nested));
  REQUIRE(file_access_make_directories(nested) == APP_OK);
  REQUIRE(file_access_make_directories(nested) == APP_OK);
  FileAccessKind kind = FILE_ACCESS_OTHER;
  REQUIRE(file_access_kind(nested, &kind) == APP_OK &&
          kind == FILE_ACCESS_DIRECTORY);
  REQUIRE(file_access_join(nested, "空文件", path, sizeof(path)) == APP_OK);
  REQUIRE(file_access_write(path, nullptr, 0) == APP_OK);
  char *data = nullptr;
  size_t size = 99;
  REQUIRE(file_access_read(path, &data, &size) == APP_OK && size == 0 &&
          data[0] == '\0');
  file_access_free(data);
  REQUIRE(file_access_make_directories(path) == APP_ERR_IO);
  REQUIRE(file_access_visit_directory(path, count_entry, &size) == APP_ERR_IO);
  unsigned char bytes[10000];
  for (size_t i = 0; i < sizeof(bytes); i++)
    bytes[i] = (unsigned char)i;
  REQUIRE(file_access_write(path, bytes, sizeof(bytes)) == APP_OK);
  REQUIRE(file_access_read(path, &data, &size) == APP_OK);
  REQUIRE(size == sizeof(bytes) && !memcmp(data, bytes, size) &&
          data[size] == '\0');
  file_access_free(data);
  size_t count = 0;
  REQUIRE(file_access_visit_directory(nested, count_entry, &count) == APP_OK &&
          count == 1);
  count = 0;
  REQUIRE(file_access_visit_directory(nested, stop_entry, &count) ==
              APP_ERR_INVALID_DATA &&
          count == 1);
  char output[64] = "unchanged";
  REQUIRE(file_access_join(root, "a", output, 1) == APP_ERR_INVALID_ARGUMENT);
  REQUIRE(!strcmp(output, "unchanged"));
  const char *bad[] = {"/absolute", "a/b", "a\\b", "C:drive", ".", "..", ""};
  for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); i++)
    REQUIRE(file_access_join(root, bad[i], output, sizeof(output)) ==
            APP_ERR_INVALID_ARGUMENT);
  REQUIRE(file_access_join(output, "a", output, sizeof(output)) ==
          APP_ERR_INVALID_ARGUMENT);
  REQUIRE(!strcmp(output, "unchanged"));
  REQUIRE(file_access_remove(nested) == APP_ERR_IO);
  REQUIRE(file_access_write(nested, "x", 1) == APP_ERR_IO);
  data = output;
  size = 99;
  REQUIRE(file_access_read(nested, &data, &size) == APP_ERR_IO &&
          data == output && size == 99);
#ifndef _WIN32
  /* POSIX 目录可包含 Windows 不合法的名称；枚举仍须把名字交给调用方。 */
  char odd[4096];
  test_path(nested, "odd:entry", odd, sizeof(odd));
  REQUIRE(file_access_write(odd, nullptr, 0) == APP_OK);
  count = 0;
  REQUIRE(file_access_visit_directory(nested, count_entry, &count) == APP_OK &&
          count == 2);
  REQUIRE(file_access_remove(odd) == APP_OK);
  char link[4096];
  test_path(root, "link", link, sizeof(link));
  REQUIRE(symlink(nested, link) == 0);
  REQUIRE(file_access_kind(link, &kind) == APP_OK && kind == FILE_ACCESS_OTHER);
  REQUIRE(file_access_visit_directory(link, count_entry, &count) == APP_ERR_IO);
  REQUIRE(file_access_remove(link) == APP_ERR_IO);
  REQUIRE(unlink(link) == 0);
  if (geteuid() != 0) {
    REQUIRE(chmod(path, 0000) == 0);
    REQUIRE(file_access_read(path, &data, &size) == APP_ERR_IO);
    REQUIRE(file_access_write(path, "x", 1) == APP_ERR_IO);
    REQUIRE(chmod(path, 0600) == 0);
  }
#endif
  REQUIRE(file_access_remove(path) == APP_OK);
  REQUIRE(file_access_remove(path) == APP_ERR_NOT_FOUND);
  REQUIRE(file_access_read(path, &data, &size) == APP_ERR_NOT_FOUND &&
          data == output && size == 99);
  kind = FILE_ACCESS_OTHER;
  REQUIRE(file_access_kind(path, &kind) == APP_ERR_NOT_FOUND &&
          kind == FILE_ACCESS_OTHER);
  REQUIRE(file_access_visit_directory(path, count_entry, &count) ==
          APP_ERR_NOT_FOUND);
  REQUIRE(file_access_read(nullptr, &data, &size) == APP_ERR_INVALID_ARGUMENT);
  REQUIRE(file_access_write(path, nullptr, 1) == APP_ERR_INVALID_ARGUMENT);
  REQUIRE(file_access_kind(root, nullptr) == APP_ERR_INVALID_ARGUMENT);
  REQUIRE(file_access_visit_directory(root, nullptr, nullptr) ==
          APP_ERR_INVALID_ARGUMENT);
  file_access_free(nullptr);
  test_remove_tree(root);
  puts("test_file_access: passed");
  return 0;
}
