#include "password.h"

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

// 有界检查避免为拒绝超长输入而扫描整个字符串；长度按字节计算。

static size_t password_length(const char *password) {
  size_t length = 0;
  while (length <= PASSWORD_MAX_BYTES && password[length] != '\0') {
    length++;
  }
  return length;
}

static AppResult crypto_failure() {
  AppResult result = APP_ERR_IO;
  unsigned long error;
  while ((error = ERR_get_error()) != 0) {
    if (ERR_GET_REASON(error) == ERR_R_MALLOC_FAILURE) {
      result = APP_ERR_NO_MEMORY;
    }
  }
  return result;
}

static AppResult derive(const char *password, const size_t length,
                        const unsigned char salt[PASSWORD_SALT_BYTES],
                        unsigned char hash[PASSWORD_HASH_BYTES]) {
  // 清除旧错误，防止把调用方遗留的错误误判为本次派生失败原因。

  ERR_clear_error();
  if (PKCS5_PBKDF2_HMAC(password, (int)length, salt, PASSWORD_SALT_BYTES,
                        PASSWORD_PBKDF2_ITERATIONS, EVP_sha256(),
                        PASSWORD_HASH_BYTES, hash) != 1) {
    return crypto_failure();
  }
  return APP_OK;
}

static void encode_hex(const unsigned char *bytes, const size_t count,
                       char *out) {
  static constexpr char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < count; i++) {
    out[2 * i] = digits[bytes[i] >> 4];
    out[2 * i + 1] = digits[bytes[i] & 15];
  }
  out[2 * count] = '\0';
}

static int hex_digit(const unsigned char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  return -1;
}

static bool decode_hex(const char *text, const size_t count,
                       unsigned char *out) {
  // 先确认长度，遇到短字符串时不访问其末尾 NUL 之后的内存。

  size_t length = 0;
  while (length < 2 * count && text[length] != '\0') {
    length++;
  }
  if (length != 2 * count || text[length] != '\0') {
    return false;
  }
  for (size_t i = 0; i < count; i++) {
    const int high = hex_digit((unsigned char)text[2 * i]);
    const int low = hex_digit((unsigned char)text[2 * i + 1]);
    if (high < 0 || low < 0) {
      return false;
    }
    out[i] = (unsigned char)((high << 4) | low);
  }
  return true;
}

AppResult password_create(const char *password,
                          char salt_hex_out[PASSWORD_SALT_HEX_LENGTH + 1],
                          char hash_hex_out[PASSWORD_HASH_HEX_LENGTH + 1]) {
  if (password == nullptr || salt_hex_out == nullptr ||
      hash_hex_out == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  const size_t length = password_length(password);
  if (length < PASSWORD_MIN_BYTES || length > PASSWORD_MAX_BYTES) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  unsigned char salt[PASSWORD_SALT_BYTES] = {0};
  unsigned char hash[PASSWORD_HASH_BYTES] = {0};
  ERR_clear_error();
  AppResult result;
  if (RAND_bytes(salt, sizeof salt) != 1) {
    result = crypto_failure();
  } else {
    result = derive(password, length, salt, hash);
  }

  // 全部操作成功后才写输出，随机源或派生失败不破坏调用方原值。

  if (result == APP_OK) {
    encode_hex(salt, sizeof salt, salt_hex_out);
    encode_hex(hash, sizeof hash, hash_hex_out);
  }
  OPENSSL_cleanse(salt, sizeof salt);
  OPENSSL_cleanse(hash, sizeof hash);
  return result;
}

AppResult password_verify(const char *password, const char *salt_hex,
                          const char *hash_hex) {
  if (password == nullptr || salt_hex == nullptr || hash_hex == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  const size_t length = password_length(password);
  if (length < PASSWORD_MIN_BYTES || length > PASSWORD_MAX_BYTES) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  unsigned char salt[PASSWORD_SALT_BYTES] = {0};
  unsigned char expected[PASSWORD_HASH_BYTES] = {0};
  unsigned char actual[PASSWORD_HASH_BYTES] = {0};
  AppResult result = APP_ERR_INVALID_DATA;
  if (decode_hex(salt_hex, sizeof salt, salt) &&
      decode_hex(hash_hex, sizeof expected, expected)) {
    result = derive(password, length, salt, actual);
    if (result == APP_OK &&
        CRYPTO_memcmp(actual, expected, sizeof actual) != 0) {
      result = APP_ERR_AUTH_FAILED;
    }
  }
  OPENSSL_cleanse(salt, sizeof salt);
  OPENSSL_cleanse(expected, sizeof expected);
  OPENSSL_cleanse(actual, sizeof actual);
  return result;
}
