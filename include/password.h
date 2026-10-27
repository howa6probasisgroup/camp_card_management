#ifndef CAMP_CARD_PASSWORD_H
#define CAMP_CARD_PASSWORD_H

#include "common.h"
#include <stdbool.h>

/// OpenSSL Crypto >= 3.0 实现；RAND_bytes 随机盐，PBKDF2-HMAC-SHA-256，
/// 200000 次迭代、32 字节输出，小写 hex。密码 8..64 字节，不保存明文。
/// 输出缓冲区须分别至少 33/65 字节且互不重叠；失败保持原值。
/// OpenSSL 随机/派生失败映射 APP_ERR_IO，分配失败 NO_MEMORY。
AppResult password_create(const char *password,
                          char salt_hex_out[PASSWORD_SALT_HEX_LENGTH + 1],
                          char hash_hex_out[PASSWORD_HASH_HEX_LENGTH + 1]);
/// 校验使用恒定时间比较；不匹配、非法参数/hex 或内部失败统一 false。
bool password_verify(const char *password, const char *salt_hex,
                     const char *hash_hex);

#endif // CAMP_CARD_PASSWORD_H
