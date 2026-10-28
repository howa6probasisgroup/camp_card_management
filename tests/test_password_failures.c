#include "password.h"
#include "password_test_hooks.h"

#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// CMake 只在本测试目标中替换随机和派生入口；正式库始终使用 OpenSSL。

static bool fail_random;
static int failure_reason;
static size_t failures;

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      failures++;                                                              \
      fprintf(stderr, "密码故障测试失败，行 %d\n", __LINE__);                  \
    }                                                                          \
  } while (0)

static void raise_failure() {
  if (failure_reason != 0) {
    ERR_raise(ERR_LIB_EVP, failure_reason);

    // 外层错误可能遮住先前的分配失败，不能只检查队尾。

    ERR_raise(ERR_LIB_EVP, ERR_R_INTERNAL_ERROR);
  }
}

int password_test_rand_bytes(unsigned char *buffer, const int count) {
  memset(buffer, 0xa5, (size_t)count);
  if (fail_random) {
    raise_failure();
    return 0;
  }
  return 1;
}

int password_test_pbkdf2(const char *, int, const unsigned char *, int, int,
                         const EVP_MD *, const int keylen, unsigned char *out) {

  // 模拟部分写入后失败，公开输出仍须保持原值。

  memset(out, 0xa5, (size_t)keylen);
  raise_failure();
  return 0;
}

int main() {
  constexpr int reasons[] = {0, ERR_R_INTERNAL_ERROR, ERR_R_MALLOC_FAILURE};
  for (size_t i = 0; i < sizeof reasons / sizeof reasons[0]; i++) {
    failure_reason = reasons[i];
    const AppResult expected =
        failure_reason == ERR_R_MALLOC_FAILURE ? APP_ERR_NO_MEMORY : APP_ERR_IO;
    for (int mode = 0; mode < 2; mode++) {
      fail_random = mode == 0;
      char salt[PASSWORD_SALT_HEX_LENGTH + 1];
      char hash[PASSWORD_HASH_HEX_LENGTH + 1];
      char old_salt[sizeof salt];
      char old_hash[sizeof hash];
      memset(salt, 's', sizeof salt);
      memset(hash, 'h', sizeof hash);
      memcpy(old_salt, salt, sizeof salt);
      memcpy(old_hash, hash, sizeof hash);

      // 遗留的分配失败不应影响本次 IO 错误映射。

      ERR_raise(ERR_LIB_EVP, ERR_R_MALLOC_FAILURE);
      CHECK(password_create("test-passphrase", salt, hash) == expected);
      CHECK(memcmp(salt, old_salt, sizeof salt) == 0);
      CHECK(memcmp(hash, old_hash, sizeof hash) == 0);
    }
    ERR_raise(ERR_LIB_EVP, ERR_R_MALLOC_FAILURE);
    CHECK(password_verify("test-passphrase", "000102030405060708090a0b0c0d0e0f",
                          "00000000000000000000000000000000"
                          "00000000000000000000000000000000") == expected);
  }
  if (failures != 0) {
    return EXIT_FAILURE;
  }
  puts("密码故障测试通过");
  return EXIT_SUCCESS;
}
