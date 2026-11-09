#ifndef CAMP_CARD_PASSWORD_H
#define CAMP_CARD_PASSWORD_H

#include "common.h"

/**
 * @brief OpenSSL Crypto >= 3.0 实现；
 * @details RAND_bytes 随机盐，PBKDF2-HMAC-SHA-256， 200000 次迭代、32 字节输出，小写 hex。密码 8..64 字节，不保存明文。
 *          输出缓冲区须分别至少 33/65 字节且互不重叠；失败保持原值。 OpenSSL 随机/派生失败映射 `APP_ERR_IO`，分配失败 `APP_ERR_NO_MEMORY`。
 * @param[in] password 待派生凭据的明文密码；只在调用期间使用。
 * @param[out] salt_hex_out 接收小写十六进制随机盐的缓冲区，容量至少为 PASSWORD_SALT_HEX_LENGTH + 1。
 * @param[out] hash_hex_out 接收小写十六进制派生值的缓冲区，容量至少为 PASSWORD_HASH_HEX_LENGTH + 1。
 * @return `APP_OK` 表示盐和派生值均已生成；参数不合法返回 `APP_ERR_INVALID_ARGUMENT`，OpenSSL 随机或派生失败返回 `APP_ERR_IO`，分配失败返回
 *         `APP_ERR_NO_MEMORY`。
 */
AppResult password_create(const char *password,
                          char salt_hex_out[PASSWORD_SALT_HEX_LENGTH + 1],
                          char hash_hex_out[PASSWORD_HASH_HEX_LENGTH + 1]);
/**
 * @brief 使用恒定时间比较；
 * @details 匹配返回 `APP_OK`，不匹配返回 `APP_ERR_AUTH_FAILED`。 NULL 或密码长度不合法返回
 *          `APP_ERR_INVALID_ARGUMENT`；盐或派生值格式错误 返回 `APP_ERR_INVALID_DATA`；派生失败返回
 *          `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in] password 待验证的明文密码；只在调用期间使用。
 * @param[in] salt_hex 创建凭据时保存的小写十六进制盐。
 * @param[in] hash_hex 与盐对应的小写十六进制密码派生值。
 * @return `APP_OK` 表示密码匹配；不匹配返回 `APP_ERR_AUTH_FAILED`，参数无效返回 `APP_ERR_INVALID_ARGUMENT`，凭据格式损坏返回
 *         `APP_ERR_INVALID_DATA`，派生失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult password_verify(const char *password, const char *salt_hex,
                          const char *hash_hex);

#endif // CAMP_CARD_PASSWORD_H
