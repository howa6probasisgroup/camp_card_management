#ifndef CAMP_CARD_COMMON_H
#define CAMP_CARD_COMMON_H

/// 学号的 ASCII 数字位数。
#define STUDENT_ID_LENGTH 9
/// 用户名最大字节数，不含末尾 NUL。
#define USERNAME_MAX_BYTES 48
/// 姓名最大 UTF-8 字节数，不含末尾 NUL。
#define NAME_MAX_BYTES 96
/// 性别字段最大字节数，不含末尾 NUL。
#define SEX_MAX_BYTES 15
/// 院系最大 UTF-8 字节数，不含末尾 NUL。
#define DEPARTMENT_MAX_BYTES 96
/// 班级最大 UTF-8 字节数，不含末尾 NUL。
#define CLASS_NAME_MAX_BYTES 64
/// 电话最大字节数，不含末尾 NUL。
#define PHONE_MAX_BYTES 24
/// 密码最小字节数。
#define PASSWORD_MIN_BYTES 8
/// 密码最大字节数。
#define PASSWORD_MAX_BYTES 64
/// 随机盐的字节数。
#define PASSWORD_SALT_BYTES 16
/// 密码派生值的字节数。
#define PASSWORD_HASH_BYTES 32
/// 盐的十六进制字符数，不含末尾 NUL。
#define PASSWORD_SALT_HEX_LENGTH 32
/// 派生值的十六进制字符数，不含末尾 NUL。
#define PASSWORD_HASH_HEX_LENGTH 64
/// PBKDF2-HMAC-SHA-256 迭代次数。
#define PASSWORD_PBKDF2_ITERATIONS 200000

/// 统一操作结果；参数指针为空返回
/// APP_ERR_INVALID_ARGUMENT，失败时标量和模型输出保持原值。
/// 模块不打印错误或秘密；由 CLI 根据返回码提示。
typedef enum {
  APP_OK = 0,
  APP_ERR_INVALID_ARGUMENT,
  APP_ERR_NOT_FOUND,
  APP_ERR_ALREADY_EXISTS,
  APP_ERR_AUTH_FAILED,
  APP_ERR_PERMISSION_DENIED,
  APP_ERR_INSUFFICIENT_BALANCE,
  APP_ERR_CARD_FROZEN,
  APP_ERR_IO,
  APP_ERR_INVALID_DATA,
  APP_ERR_NO_MEMORY
} AppResult;

typedef enum {
  ROLE_ADMIN = 0,
  ROLE_CARD_USER = 1,
  ROLE_CARD_CLERK = 2
} UserRole;

typedef enum { CARD_STATUS_ACTIVE = 0, CARD_STATUS_FROZEN = 1 } CardStatus;

#endif // CAMP_CARD_COMMON_H
