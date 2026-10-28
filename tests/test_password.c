#include "password.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t failures;

// 仅输出位置，不把测试表达式中的密码或凭据写入日志。

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      failures++;                                                              \
      fprintf(stderr, "密码测试失败，行 %d\n", __LINE__);                      \
    }                                                                          \
  } while (0)

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

static void test_round_trip() {
  char salt[PASSWORD_SALT_HEX_LENGTH + 1] = {0};
  char hash[PASSWORD_HASH_HEX_LENGTH + 1] = {0};
  char other_salt[sizeof salt] = {0};
  char other_hash[sizeof hash] = {0};
  CHECK(password_create("test-passphrase", salt, hash) == APP_OK);
  CHECK(is_lower_hex(salt, PASSWORD_SALT_HEX_LENGTH));
  CHECK(is_lower_hex(hash, PASSWORD_HASH_HEX_LENGTH));
  CHECK(password_verify("test-passphrase", salt, hash) == APP_OK);
  CHECK(password_verify("wrong-passphrase", salt, hash) == APP_ERR_AUTH_FAILED);
  CHECK(password_create("test-passphrase", other_salt, other_hash) == APP_OK);
  CHECK(strcmp(salt, other_salt) != 0);
  CHECK(strcmp(hash, other_hash) != 0);
  CHECK(password_verify("test-passphrase", other_salt, other_hash) == APP_OK);

  // 修改末尾派生字节也必须导致认证失败。

  hash[PASSWORD_HASH_HEX_LENGTH - 1] =
      hash[PASSWORD_HASH_HEX_LENGTH - 1] == '0' ? '1' : '0';
  CHECK(password_verify("test-passphrase", salt, hash) == APP_ERR_AUTH_FAILED);
}

static void test_lengths_and_arguments() {
  char password[PASSWORD_MAX_BYTES + 2];
  char salt[PASSWORD_SALT_HEX_LENGTH + 1];
  char hash[PASSWORD_HASH_HEX_LENGTH + 1];
  constexpr size_t lengths[] = {0, PASSWORD_MIN_BYTES - 1, PASSWORD_MIN_BYTES,
                                PASSWORD_MAX_BYTES, PASSWORD_MAX_BYTES + 1};
  for (size_t i = 0; i < sizeof lengths / sizeof lengths[0]; i++) {
    memset(password, 'x', lengths[i]);
    password[lengths[i]] = '\0';
    memset(salt, 's', sizeof salt);
    memset(hash, 'h', sizeof hash);
    const bool valid =
        lengths[i] >= PASSWORD_MIN_BYTES && lengths[i] <= PASSWORD_MAX_BYTES;
    CHECK(password_create(password, salt, hash) ==
          (valid ? APP_OK : APP_ERR_INVALID_ARGUMENT));
    if (valid) {
      CHECK(password_verify(password, salt, hash) == APP_OK);
    } else {
      // 比较整个输出区，包括末尾，检查失败没有部分写入。

      char expected_salt[sizeof salt];
      char expected_hash[sizeof hash];
      memset(expected_salt, 's', sizeof expected_salt);
      memset(expected_hash, 'h', sizeof expected_hash);
      CHECK(memcmp(salt, expected_salt, sizeof salt) == 0);
      CHECK(memcmp(hash, expected_hash, sizeof hash) == 0);
      CHECK(password_verify(password, "", "") == APP_ERR_INVALID_ARGUMENT);
    }
  }
  CHECK(password_create(nullptr, salt, hash) == APP_ERR_INVALID_ARGUMENT);
  CHECK(password_create("test-passphrase", nullptr, hash) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(password_create("test-passphrase", salt, nullptr) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(password_verify(nullptr, "", "") == APP_ERR_INVALID_ARGUMENT);
  CHECK(password_verify("test-passphrase", nullptr, "") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(password_verify("test-passphrase", "", nullptr) ==
        APP_ERR_INVALID_ARGUMENT);

  // 三个汉字为九个 UTF-8 字节，长度校验不按字符数计算。

  CHECK(password_create("虚构值", salt, hash) == APP_OK);
  CHECK(password_verify("虚构值", salt, hash) == APP_OK);
}

static void test_encoding() {
  char salt[PASSWORD_SALT_HEX_LENGTH + 1] = {0};
  char hash[PASSWORD_HASH_HEX_LENGTH + 1] = {0};
  CHECK(password_create("test-passphrase", salt, hash) == APP_OK);
  CHECK(password_verify("test-passphrase", "", hash) == APP_ERR_INVALID_DATA);
  CHECK(password_verify("test-passphrase", salt, "") == APP_ERR_INVALID_DATA);
  CHECK(password_verify("test-passphrase", salt + 1, hash) ==
        APP_ERR_INVALID_DATA);
  CHECK(password_verify("test-passphrase", salt, hash + 1) ==
        APP_ERR_INVALID_DATA);
  char long_salt[PASSWORD_SALT_HEX_LENGTH + 2];
  char long_hash[PASSWORD_HASH_HEX_LENGTH + 2];
  memset(long_salt, '0', sizeof long_salt - 1);
  memset(long_hash, '0', sizeof long_hash - 1);
  long_salt[sizeof long_salt - 1] = '\0';
  long_hash[sizeof long_hash - 1] = '\0';
  CHECK(password_verify("test-passphrase", long_salt, hash) ==
        APP_ERR_INVALID_DATA);
  CHECK(password_verify("test-passphrase", salt, long_hash) ==
        APP_ERR_INVALID_DATA);

  constexpr unsigned char invalid[] = {'A', 'F', 'g', '/', ' ', 0xff};
  for (size_t i = 0; i < sizeof invalid; i++) {
    // 分别覆盖首尾、两个半字节位置，避免只检查凭据前缀。

    for (size_t position = 0; position < PASSWORD_HASH_HEX_LENGTH; position++) {
      const char saved = hash[position];
      hash[position] = (char)invalid[i];
      CHECK(password_verify("test-passphrase", salt, hash) ==
            APP_ERR_INVALID_DATA);
      hash[position] = saved;
    }
    for (size_t position = 0; position < PASSWORD_SALT_HEX_LENGTH; position++) {
      const char saved = salt[position];
      salt[position] = (char)invalid[i];
      CHECK(password_verify("test-passphrase", salt, hash) ==
            APP_ERR_INVALID_DATA);
      salt[position] = saved;
    }
  }
}

static void test_known_vector() {
  // 固定测试向量独立于本模块生成，用于检查摘要、迭代次数及盐解码。

  const char *salt = "000102030405060708090a0b0c0d0e0f";
  const char *hash =
      "78c250765d329b36e5bcbf133bf840b491e23d8d0de1c32865d355282e8ab12b";
  CHECK(password_verify("test-passphrase", salt, hash) == APP_OK);
  CHECK(password_verify("wrong-passphrase", salt, hash) == APP_ERR_AUTH_FAILED);
}

int main() {
  test_round_trip();
  test_lengths_and_arguments();
  test_encoding();
  test_known_vector();
  if (failures != 0) {
    fprintf(stderr, "密码测试失败项数：%zu\n", failures);
    return EXIT_FAILURE;
  }
  puts("密码测试通过");
  return EXIT_SUCCESS;
}
