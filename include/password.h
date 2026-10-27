#ifndef CAMP_CARD_PASSWORD_H
#define CAMP_CARD_PASSWORD_H

#include "common.h"

/// OpenSSL Crypto >= 3.0 实现；RAND_bytes 随机盐，PBKDF2-HMAC-SHA-256，
/// 200000 次迭代、32 字节输出，小写 hex。密码 8..64 字节，不保存明文。
/// 输出缓冲区须分别至少 33/65 字节且互不重叠；失败保持原值。
/// OpenSSL 随机/派生失败映射 APP_ERR_IO，分配失败 NO_MEMORY。
AppResult password_create(const char *password,
                          char salt_hex_out[PASSWORD_SALT_HEX_LENGTH + 1],
                          char hash_hex_out[PASSWORD_HASH_HEX_LENGTH + 1]);
/// 使用恒定时间比较；匹配返回 APP_OK，不匹配返回 APP_ERR_AUTH_FAILED。
/// NULL 或密码长度不合法返回 APP_ERR_INVALID_ARGUMENT；盐或派生值格式错误
/// 返回 APP_ERR_INVALID_DATA；派生失败返回 APP_ERR_IO，分配失败返回
/// APP_ERR_NO_MEMORY。
AppResult password_verify(const char *password, const char *salt_hex,
                          const char *hash_hex);

#endif // CAMP_CARD_PASSWORD_H
