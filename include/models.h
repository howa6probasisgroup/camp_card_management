#ifndef CAMP_CARD_MODELS_H
#define CAMP_CARD_MODELS_H

#include "common.h"
#include <stdint.h>

/**
 * @brief 学生及其校园卡的输入信息。
 * @details 学号由 9 位 ASCII 数字组成；姓名、院系和班级是非空 UTF-8
 * 字符串。姓名按原始字节匹配，性别仅允许“男”“女”“其他”，电话可空。
 */
typedef struct {
  /**
   * @brief 九位 ASCII 数字并以 NUL 结尾，卡号与学号相同。
   * @details 卡号不单独存储；字符数组还须为末尾 NUL 预留一个字节。
   */
  char student_id[STUDENT_ID_LENGTH + 1];
  /**
   * @brief 非空 UTF-8 姓名，以 NUL 结尾；
   * @details 按原始字节精确匹配。
   */
  char name[NAME_MAX_BYTES + 1];
  /**
   * @brief 仅允许“男”“女”“其他”，以 NUL 结尾。
   * @details 只能使用“男”“女”或“其他”，并以 NUL 结尾。
   */
  char sex[SEX_MAX_BYTES + 1];
  /**
   * @brief 非空 UTF-8 院系，以 NUL 结尾。
   * @details 值非空，按 UTF-8 编码保存，最大字节数由 DEPARTMENT_MAX_BYTES
   * 限定。
   */
  char department[DEPARTMENT_MAX_BYTES + 1];
  /**
   * @brief 非空 UTF-8 班级，以 NUL 结尾。
   * @details 值非空，按 UTF-8 编码保存，最大字节数由 CLASS_NAME_MAX_BYTES
   * 限定。
   */
  char class_name[CLASS_NAME_MAX_BYTES + 1];
  /**
   * @brief 允许空字符串；
   * @details 非空时仅含 ASCII 数字、+、-、空格，以 NUL 结尾。
   */
  char phone[PHONE_MAX_BYTES + 1];
} StudentProfileInput;

/**
 * @brief 学生及其校园卡的主记录。
 * @details 学号同时作为卡号；余额以整数分保存，状态由 CardStatus 表示。
 */
typedef struct {
  /**
   * @brief 九位 ASCII 数字并以 NUL 结尾，卡号与学号相同。
   * @details 卡号不单独存储；字符数组还须为末尾 NUL 预留一个字节。
   */
  char student_id[STUDENT_ID_LENGTH + 1];
  /**
   * @brief 非空 UTF-8 姓名，以 NUL 结尾；
   * @details 按原始字节精确匹配。
   */
  char name[NAME_MAX_BYTES + 1];
  /**
   * @brief 仅允许“男”“女”“其他”，以 NUL 结尾。
   * @details 只能使用“男”“女”或“其他”，并以 NUL 结尾。
   */
  char sex[SEX_MAX_BYTES + 1];
  /**
   * @brief 非空 UTF-8 院系，以 NUL 结尾。
   * @details 值非空，按 UTF-8 编码保存，最大字节数由 DEPARTMENT_MAX_BYTES
   * 限定。
   */
  char department[DEPARTMENT_MAX_BYTES + 1];
  /**
   * @brief 非空 UTF-8 班级，以 NUL 结尾。
   * @details 值非空，按 UTF-8 编码保存，最大字节数由 CLASS_NAME_MAX_BYTES
   * 限定。
   */
  char class_name[CLASS_NAME_MAX_BYTES + 1];
  /**
   * @brief 允许空字符串；
   * @details 非空时仅含 ASCII 数字、+、-、空格，以 NUL 结尾。
   */
  char phone[PHONE_MAX_BYTES + 1];
  /**
   * @brief 非负整数分；
   * @details 运算必须检查溢出。
   */
  int64_t balance_cents;
  /**
   * @brief 校园卡当前状态。
   * @details 仅使用 `CARD_STATUS_ACTIVE` 或 `CARD_STATUS_FROZEN`。
   */
  CardStatus status;
} StudentCardRecord;

/**
 * @brief username: 1..48 字节，仅 ASCII 字母、数字、_、-、.，区分大小写。
 * @details card_user 的 student_id 必须关联已开卡记录；其余角色为空字符串。
 * 盐和派生值为小写十六进制；Account 含凭据，不得整体打印或记录。
 */
typedef struct {
  /**
   * @brief 账号用户名。
   * @details 长度为 1..48 字节，仅允许 ASCII
   * 字母、数字、下划线、连字符和句点，区分大小写。
   */
  char username[USERNAME_MAX_BYTES + 1];
  /**
   * @brief 密码盐的小写十六进制文本。
   * @details 内容为 PASSWORD_SALT_BYTES
   * 个随机字节的十六进制表示，不包含明文密码。
   */
  char password_salt_hex[PASSWORD_SALT_HEX_LENGTH + 1];
  /**
   * @brief 密码派生值的小写十六进制文本。
   * @details 使用公共密码参数派生保存，不得向用户输出或写入日志。
   */
  char password_hash_hex[PASSWORD_HASH_HEX_LENGTH + 1];
  /**
   * @brief 账号角色。
   * @details 取值由 UserRole 定义，并决定可访问的业务权限。
   */
  UserRole role;
  /**
   * @brief 卡用户须关联已开卡学号；
   * @details 其他角色为空字符串。
   */
  char student_id[STUDENT_ID_LENGTH + 1];
} Account;

#endif // CAMP_CARD_MODELS_H
