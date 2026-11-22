#ifndef _WIN32
// POSIX 规定的功能开关名称，不能重命名。
// NOLINTNEXTLINE(bugprone-reserved-identifier)
#define _POSIX_C_SOURCE 200809L
#endif
#include "file_access.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool separator(const char c) {
#ifdef _WIN32
  return c == '/' || c == '\\';
#else
  return c == '/';
#endif
}

#ifdef _WIN32
/* 公共路径统一为 UTF-8，Windows 文件接口使用 UTF-16。 */
static AppResult wide_path(const char *path, wchar_t **out) {
  const int n =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
  if (!n)
    return APP_ERR_INVALID_ARGUMENT;
  wchar_t *input = malloc((size_t)n * sizeof(*input));
  if (!input)
    return APP_ERR_NO_MEMORY;
  if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, input, n)) {
    free(input);
    return APP_ERR_INVALID_ARGUMENT;
  }

  const bool extended =
      !wcsncmp(input, L"\\\\?\\", 4) || !wcsncmp(input, L"\\\\.\\", 4);
  wchar_t *absolute = input;
  if (!extended) {
    const DWORD needed = GetFullPathNameW(input, 0, nullptr, nullptr);
    if (!needed) {
      free(input);
      return APP_ERR_INVALID_ARGUMENT;
    }
    absolute = malloc((size_t)needed * sizeof(*absolute));
    if (!absolute) {
      free(input);
      return APP_ERR_NO_MEMORY;
    }
    const DWORD written = GetFullPathNameW(input, needed, absolute, nullptr);
    free(input);
    if (!written || written >= needed) {
      free(absolute);
      return APP_ERR_INVALID_ARGUMENT;
    }
  }
  for (wchar_t *p = absolute; *p; p++)
    if (*p == L'/')
      *p = L'\\';

  if (extended) {
    *out = absolute;
    return APP_OK;
  }

  const bool unc = absolute[0] == L'\\' && absolute[1] == L'\\';
  const wchar_t *prefix = unc ? L"\\\\?\\UNC\\" : L"\\\\?\\";
  const wchar_t *body = unc ? absolute + 2 : absolute;
  const size_t prefix_length = wcslen(prefix);
  const size_t body_length = wcslen(body);
  wchar_t *result =
      malloc((prefix_length + body_length + 1) * sizeof(*result));
  if (!result) {
    free(absolute);
    return APP_ERR_NO_MEMORY;
  }
  memcpy(result, prefix, prefix_length * sizeof(*result));
  memcpy(result + prefix_length, body, (body_length + 1) * sizeof(*result));
  free(absolute);
  *out = result;
  return APP_OK;
}

static AppResult windows_error(const DWORD e) {
  return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND
             ? APP_ERR_NOT_FOUND
             : APP_ERR_IO;
}
#endif

AppResult file_access_kind(const char *path, FileAccessKind *out) {
  if (!path || !*path || !out)
    return APP_ERR_INVALID_ARGUMENT;
  FileAccessKind kind;
#ifdef _WIN32
  wchar_t *wide;
  const AppResult r = wide_path(path, &wide);
  if (r != APP_OK)
    return r;
  const DWORD a = GetFileAttributesW(wide);
  const DWORD error = GetLastError();
  free(wide);
  if (a == INVALID_FILE_ATTRIBUTES)
    return windows_error(error);
  kind = a & FILE_ATTRIBUTE_REPARSE_POINT ? FILE_ACCESS_OTHER
         : a & FILE_ATTRIBUTE_DIRECTORY   ? FILE_ACCESS_DIRECTORY
                                          : FILE_ACCESS_REGULAR;
#else
  struct stat info;
  if (lstat(path, &info) != 0)
    return errno == ENOENT ? APP_ERR_NOT_FOUND : APP_ERR_IO;
  kind = S_ISREG(info.st_mode)   ? FILE_ACCESS_REGULAR
         : S_ISDIR(info.st_mode) ? FILE_ACCESS_DIRECTORY
                                 : FILE_ACCESS_OTHER;
#endif
  *out = kind;
  return APP_OK;
}

AppResult file_access_same_directory(const char *first, const char *second,
                                     bool *out) {
  if (!first || !*first || !second || !*second || !out)
    return APP_ERR_INVALID_ARGUMENT;
#ifdef _WIN32
  wchar_t *wide1 = nullptr;
  AppResult r = wide_path(first, &wide1);
  if (r != APP_OK)
    return r;
  wchar_t *wide2 = nullptr;
  r = wide_path(second, &wide2);
  if (r != APP_OK) {
    free(wide1);
    return r;
  }
  const DWORD a1 = GetFileAttributesW(wide1);
  const DWORD err1 = GetLastError();
  if (a1 == INVALID_FILE_ATTRIBUTES) {
    free(wide1);
    free(wide2);
    return windows_error(err1);
  }
  if (a1 & FILE_ATTRIBUTE_REPARSE_POINT || !(a1 & FILE_ATTRIBUTE_DIRECTORY)) {
    free(wide1);
    free(wide2);
    return APP_ERR_INVALID_DATA;
  }
  const DWORD a2 = GetFileAttributesW(wide2);
  const DWORD err2 = GetLastError();
  if (a2 == INVALID_FILE_ATTRIBUTES) {
    free(wide1);
    free(wide2);
    return windows_error(err2);
  }
  if (a2 & FILE_ATTRIBUTE_REPARSE_POINT || !(a2 & FILE_ATTRIBUTE_DIRECTORY)) {
    free(wide1);
    free(wide2);
    return APP_ERR_INVALID_DATA;
  }
  HANDLE h1 = CreateFileW(
      wide1, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  free(wide1);
  if (h1 == INVALID_HANDLE_VALUE) {
    free(wide2);
    return APP_ERR_IO;
  }
  HANDLE h2 = CreateFileW(
      wide2, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  free(wide2);
  if (h2 == INVALID_HANDLE_VALUE) {
    CloseHandle(h1);
    return APP_ERR_IO;
  }
  BY_HANDLE_FILE_INFORMATION fi1;
  BY_HANDLE_FILE_INFORMATION fi2;
  if (!GetFileInformationByHandle(h1, &fi1) ||
      !GetFileInformationByHandle(h2, &fi2)) {
    CloseHandle(h1);
    CloseHandle(h2);
    return APP_ERR_IO;
  }
  CloseHandle(h1);
  CloseHandle(h2);
  *out = fi1.dwVolumeSerialNumber == fi2.dwVolumeSerialNumber &&
         fi1.nFileIndexHigh == fi2.nFileIndexHigh &&
         fi1.nFileIndexLow == fi2.nFileIndexLow;
  return APP_OK;
#else
  struct stat st1;
  if (lstat(first, &st1) != 0)
    return errno == ENOENT ? APP_ERR_NOT_FOUND : APP_ERR_IO;
  if (!S_ISDIR(st1.st_mode))
    return APP_ERR_INVALID_DATA;
  struct stat st2;
  if (lstat(second, &st2) != 0)
    return errno == ENOENT ? APP_ERR_NOT_FOUND : APP_ERR_IO;
  if (!S_ISDIR(st2.st_mode))
    return APP_ERR_INVALID_DATA;
  *out = st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino;
  return APP_OK;
#endif
}

static AppResult make_one(const char *path) {
  FileAccessKind kind;
  AppResult r = file_access_kind(path, &kind);
  if (r == APP_OK)
    return kind == FILE_ACCESS_DIRECTORY ? APP_OK : APP_ERR_IO;
  if (r != APP_ERR_NOT_FOUND)
    return r;
#ifdef _WIN32
  wchar_t *wide;
  r = wide_path(path, &wide);
  if (r != APP_OK)
    return r;
  const bool ok = CreateDirectoryW(wide, nullptr) != 0;
  free(wide);
#else
  const bool ok = mkdir(path, 0700) == 0;
#endif
  if (ok)
    return APP_OK;
  r = file_access_kind(path, &kind);
  return r == APP_OK && kind == FILE_ACCESS_DIRECTORY ? APP_OK : APP_ERR_IO;
}

AppResult file_access_make_directories(const char *path) {
  if (!path || !*path)
    return APP_ERR_INVALID_ARGUMENT;
  FileAccessKind kind;
  AppResult r = file_access_kind(path, &kind);
  if (r == APP_OK)
    return kind == FILE_ACCESS_DIRECTORY ? APP_OK : APP_ERR_IO;
  if (r != APP_ERR_NOT_FOUND)
    return r;
  size_t n = strlen(path);
  char *copy = malloc(n + 1);
  if (!copy)
    return APP_ERR_NO_MEMORY;
  memcpy(copy, path, n + 1);
  while (n > 1 && separator(copy[n - 1]))
    copy[--n] = '\0';
  size_t parent = n;
  while (parent && !separator(copy[parent - 1]))
    parent--;
  if (parent > 0) {
    size_t cut = parent > 1 ? parent - 1 : 1;
#ifdef _WIN32
    if (cut == 2 && copy[1] == ':')
      cut = 3;
#endif
    const char saved = copy[cut];
    copy[cut] = '\0';
    r = cut < n ? file_access_make_directories(copy) : APP_ERR_IO;
    copy[cut] = saved;
  } else
    r = APP_OK;
  if (r == APP_OK)
    r = make_one(copy);
  free(copy);
  return r;
}

static bool overlaps(const void *a, const size_t an, const void *b,
                     const size_t bn) {
  const uintptr_t x = (uintptr_t)a;
  const uintptr_t y = (uintptr_t)b;
  return x <= y ? y - x < an : x - y < bn;
}

AppResult file_access_join(const char *directory, const char *component,
                           char *out, const size_t capacity) {
  if (!directory || !component || !out || !*directory || !*component ||
      strchr(component, '/') || strchr(component, '\\') ||
      strchr(component, ':') || !strcmp(component, ".") ||
      !strcmp(component, ".."))
    return APP_ERR_INVALID_ARGUMENT;
  const size_t a = strlen(directory);
  const size_t b = strlen(component);
  const size_t slash = separator(directory[a - 1]) ? 0 : 1;
  if (a > SIZE_MAX - b - 2 || capacity < a + slash + b + 1 ||
      overlaps(out, capacity, directory, a + 1) ||
      overlaps(out, capacity, component, b + 1))
    return APP_ERR_INVALID_ARGUMENT;
  memcpy(out, directory, a);
  if (slash)
    out[a] = '/';
  memcpy(out + a + slash, component, b + 1);
  return APP_OK;
}

AppResult file_access_visit_directory(const char *path,
                                      const FileAccessVisitor visitor,
                                      void *context) {
  if (!path || !*path || !visitor)
    return APP_ERR_INVALID_ARGUMENT;
  FileAccessKind kind;
  AppResult r = file_access_kind(path, &kind);
  if (r != APP_OK)
    return r;
  if (kind != FILE_ACCESS_DIRECTORY)
    return APP_ERR_IO;
#ifdef _WIN32
  const size_t n = strlen(path) + 3;
  char *pattern = malloc(n);
  if (!pattern)
    return APP_ERR_NO_MEMORY;
  r = file_access_join(path, "*", pattern, n);
  wchar_t *wide = nullptr;
  if (r == APP_OK)
    r = wide_path(pattern, &wide);
  free(pattern);
  if (r != APP_OK)
    return r;
  WIN32_FIND_DATAW entry;
  HANDLE h = FindFirstFileW(wide, &entry);
  free(wide);
  if (h == INVALID_HANDLE_VALUE)
    return GetLastError() == ERROR_FILE_NOT_FOUND ? APP_OK : APP_ERR_IO;
  do {
    if (!wcscmp(entry.cFileName, L".") || !wcscmp(entry.cFileName, L".."))
      continue;
    const int bytes =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry.cFileName, -1,
                            nullptr, 0, nullptr, nullptr);
    if (!bytes) {
      r = APP_ERR_IO;
      break;
    }
    char *name = malloc((size_t)bytes);
    if (!name) {
      r = APP_ERR_NO_MEMORY;
      break;
    }
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry.cFileName, -1,
                             name, bytes, nullptr, nullptr)) {
      free(name);
      r = APP_ERR_IO;
      break;
    }
    const DWORD a = entry.dwFileAttributes;
    kind = a & FILE_ATTRIBUTE_REPARSE_POINT ? FILE_ACCESS_OTHER
           : a & FILE_ATTRIBUTE_DIRECTORY   ? FILE_ACCESS_DIRECTORY
                                            : FILE_ACCESS_REGULAR;
    r = visitor(name, kind, context);
    free(name);
    if (r != APP_OK)
      break;
  } while (FindNextFileW(h, &entry));
  if (r == APP_OK && GetLastError() != ERROR_NO_MORE_FILES)
    r = APP_ERR_IO;
  if (!FindClose(h) && r == APP_OK)
    r = APP_ERR_IO;
#else
  DIR *dir = opendir(path);
  if (!dir)
    return errno == ENOENT ? APP_ERR_NOT_FOUND : APP_ERR_IO;
  for (;;) {
    errno = 0;
    struct dirent *entry = readdir(dir);
    if (!entry) {
      if (errno)
        r = APP_ERR_IO;
      break;
    }
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
      continue;
    struct stat info;
    if (fstatat(dirfd(dir), entry->d_name, &info, AT_SYMLINK_NOFOLLOW) != 0) {
      r = errno == ENOENT ? APP_ERR_NOT_FOUND : APP_ERR_IO;
      break;
    }
    kind = S_ISREG(info.st_mode)   ? FILE_ACCESS_REGULAR
           : S_ISDIR(info.st_mode) ? FILE_ACCESS_DIRECTORY
                                   : FILE_ACCESS_OTHER;
    if (r == APP_OK)
      r = visitor(entry->d_name, kind, context);
    if (r != APP_OK)
      break;
  }
  if (closedir(dir) && r == APP_OK)
    r = APP_ERR_IO;
#endif
  return r;
}

static AppResult open_file(const char *path, const bool write, FILE **out) {
  FileAccessKind kind;
  AppResult r = file_access_kind(path, &kind);
  if (r == APP_OK && kind != FILE_ACCESS_REGULAR)
    return APP_ERR_IO;
  if (r != APP_OK && !(write && r == APP_ERR_NOT_FOUND))
    return r;
#ifdef _WIN32
  wchar_t *wide;
  r = wide_path(path, &wide);
  if (r != APP_OK)
    return r;
  FILE *file = _wfopen(wide, write ? L"wb" : L"rb");
  free(wide);
#else
  FILE *file = fopen(path, write ? "wb" : "rb");
#endif
  if (!file)
    return !write && errno == ENOENT ? APP_ERR_NOT_FOUND : APP_ERR_IO;
  *out = file;
  return APP_OK;
}

AppResult file_access_read(const char *path, char **out_data,
                           size_t *out_size) {
  if (!path || !*path || !out_data || !out_size)
    return APP_ERR_INVALID_ARGUMENT;
  FILE *file;
  AppResult r = open_file(path, false, &file);
  if (r != APP_OK)
    return r;
  size_t capacity = 4096, size = 0;
  char *data = malloc(capacity);
  if (!data)
    r = APP_ERR_NO_MEMORY;
  while (r == APP_OK) {
    const size_t count = fread(data + size, 1, capacity - size - 1, file);
    size += count;
    if (ferror(file)) {
      r = APP_ERR_IO;
      break;
    }
    if (feof(file))
      break;
    if (size == capacity - 1) {
      if (capacity > SIZE_MAX / 2) {
        r = APP_ERR_NO_MEMORY;
        break;
      }
      char *grown = realloc(data, capacity * 2);
      if (!grown) {
        r = APP_ERR_NO_MEMORY;
        break;
      }
      data = grown;
      capacity *= 2;
    }
  }
  if (fclose(file) && r == APP_OK)
    r = APP_ERR_IO;
  if (r != APP_OK) {
    free(data);
    return r;
  }
  data[size] = '\0';
  *out_data = data;
  *out_size = size;
  return APP_OK;
}

void file_access_free(void *data) { free(data); }

AppResult file_access_write(const char *path, const void *data,
                            const size_t size) {
  if (!path || !*path || (!data && size))
    return APP_ERR_INVALID_ARGUMENT;
  FILE *file;
  AppResult r = open_file(path, true, &file);
  if (r != APP_OK)
    return r;
  if (size && fwrite(data, 1, size, file) != size)
    r = APP_ERR_IO;
  if (fclose(file))
    r = APP_ERR_IO;
  return r;
}

AppResult file_access_remove(const char *path) {
  if (!path || !*path)
    return APP_ERR_INVALID_ARGUMENT;
  FileAccessKind kind;
  AppResult r = file_access_kind(path, &kind);
  if (r != APP_OK)
    return r;
  if (kind != FILE_ACCESS_REGULAR)
    return APP_ERR_IO;
#ifdef _WIN32
  wchar_t *wide;
  r = wide_path(path, &wide);
  if (r != APP_OK)
    return r;
  const bool ok = DeleteFileW(wide) != 0;
  const DWORD error = GetLastError();
  free(wide);
  return ok ? APP_OK : windows_error(error);
#else
  if (!unlink(path))
    return APP_OK;
  return errno == ENOENT ? APP_ERR_NOT_FOUND : APP_ERR_IO;
#endif
}
