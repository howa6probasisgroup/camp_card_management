#include "password.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 只验证业务登录依赖的密码契约：随机凭据可往返、错误密码被拒绝、
// 长度边界有效，以及固定向量保证派生参数稳定。

static size_t g_checks;
static size_t g_failures;

static void check(const bool condition, const int line) {
  g_checks++;
  if (!condition) {
    g_failures++;
    fprintf(stderr, "密码测试失败，行 %d\n", line);
  }
}

#define CHECK(condition) check((condition), __LINE__)

static bool is_lower_hex(const char *text, const size_t length) {
  if (strlen(text) != length) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    if (!((text[i] >= '0' && text[i] <= '9') ||
          (text[i] >= 'a' && text[i] <= 'f'))) {
      return false;
    }
  }
  return true;
}

static void test_created_credentials_round_trip() {
  char salt[PASSWORD_SALT_HEX_LENGTH + 1] = {0};
  char hash[PASSWORD_HASH_HEX_LENGTH + 1] = {0};
  char other_salt[sizeof salt] = {0};
  char other_hash[sizeof hash] = {0};

  CHECK(password_create("12345678", salt, hash) == APP_OK);
  CHECK(is_lower_hex(salt, PASSWORD_SALT_HEX_LENGTH) &&
        is_lower_hex(hash, PASSWORD_HASH_HEX_LENGTH));
  CHECK(password_verify("12345678", salt, hash) == APP_OK);
  CHECK(password_verify("12345679", salt, hash) == APP_ERR_AUTH_FAILED);

  CHECK(password_create("abcdefgh", other_salt, other_hash) == APP_OK);
  CHECK(strcmp(salt, other_salt) != 0);
  CHECK(strcmp(hash, other_hash) != 0);

  char malformed_hash[sizeof hash];
  memcpy(malformed_hash, hash, sizeof malformed_hash);
  malformed_hash[PASSWORD_HASH_HEX_LENGTH - 1] = 'g';
  CHECK(password_verify("12345678", salt, malformed_hash) ==
        APP_ERR_INVALID_DATA);
}

static void test_password_length_boundaries() {
  char salt[PASSWORD_SALT_HEX_LENGTH + 1] = {0};
  char hash[PASSWORD_HASH_HEX_LENGTH + 1] = {0};
  char short_password[PASSWORD_MIN_BYTES] = "1234567";
  char max_password[PASSWORD_MAX_BYTES + 1];
  memset(max_password, 'x', PASSWORD_MAX_BYTES);
  max_password[PASSWORD_MAX_BYTES] = '\0';
  char long_password[PASSWORD_MAX_BYTES + 2];
  memset(long_password, 'x', PASSWORD_MAX_BYTES + 1);
  long_password[PASSWORD_MAX_BYTES + 1] = '\0';

  CHECK(password_create(short_password, salt, hash) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(password_create(long_password, salt, hash) == APP_ERR_INVALID_ARGUMENT);
  CHECK(password_create(max_password, salt, hash) == APP_OK);
  CHECK(password_verify(max_password, salt, hash) == APP_OK);
}

static void test_known_vector() {
  const char *salt = "000102030405060708090a0b0c0d0e0f";
  const char *hash =
      "78c250765d329b36e5bcbf133bf840b491e23d8d0de1c32865d355282e8ab12b";

  CHECK(password_verify("test-passphrase", salt, hash) == APP_OK);
  CHECK(password_verify("wrong-passphrase", salt, hash) == APP_ERR_AUTH_FAILED);
}

int main() {
  test_created_credentials_round_trip();
  test_password_length_boundaries();
  test_known_vector();

  if (g_failures == 0) {
    printf("test_password: %zu core checks passed\n", g_checks);
    return EXIT_SUCCESS;
  }
  fprintf(stderr, "test_password: %zu of %zu checks failed\n", g_failures,
          g_checks);
  return EXIT_FAILURE;
}
