#ifndef CAMP_CARD_MODELS_H
#define CAMP_CARD_MODELS_H

#include "common.h"
#include <stdint.h>

/// 学号为 9 位 ASCII 数字，卡号等于学号；姓名/院系/班级为非空 UTF-8。
/// 姓名按原始字节精确匹配，不做规范化。sex 仅为“男”“女”“其他”。
/// phone 可空，否则仅含 ASCII 数字、+、-、空格。
typedef struct {
  /// 九位 ASCII 数字并以 NUL 结尾，卡号与学号相同。
  char student_id[STUDENT_ID_LENGTH + 1];
  /// 非空 UTF-8 姓名，以 NUL 结尾；按原始字节精确匹配。
  char name[NAME_MAX_BYTES + 1];
  /// 仅允许“男”“女”“其他”，以 NUL 结尾。
  char sex[SEX_MAX_BYTES + 1];
  /// 非空 UTF-8 院系，以 NUL 结尾。
  char department[DEPARTMENT_MAX_BYTES + 1];
  /// 非空 UTF-8 班级，以 NUL 结尾。
  char class_name[CLASS_NAME_MAX_BYTES + 1];
  /// 允许空字符串；非空时仅含 ASCII 数字、+、-、空格，以 NUL 结尾。
  char phone[PHONE_MAX_BYTES + 1];
} StudentProfileInput;

typedef struct {
  /// 九位 ASCII 数字并以 NUL 结尾，卡号与学号相同。
  char student_id[STUDENT_ID_LENGTH + 1];
  /// 非空 UTF-8 姓名，以 NUL 结尾；按原始字节精确匹配。
  char name[NAME_MAX_BYTES + 1];
  /// 仅允许“男”“女”“其他”，以 NUL 结尾。
  char sex[SEX_MAX_BYTES + 1];
  /// 非空 UTF-8 院系，以 NUL 结尾。
  char department[DEPARTMENT_MAX_BYTES + 1];
  /// 非空 UTF-8 班级，以 NUL 结尾。
  char class_name[CLASS_NAME_MAX_BYTES + 1];
  /// 允许空字符串；非空时仅含 ASCII 数字、+、-、空格，以 NUL 结尾。
  char phone[PHONE_MAX_BYTES + 1];
  int64_t balance_cents; ///< 非负整数分；运算必须检查溢出。
  CardStatus status;
} StudentCardRecord;

/// username: 1..48 字节，仅 ASCII 字母、数字、_、-、.，区分大小写。
/// card_user 的 student_id 必须关联已开卡记录；其余角色为空字符串。
/// 盐和派生值为小写十六进制；Account 含凭据，不得整体打印或记录。
typedef struct {
  char username[USERNAME_MAX_BYTES + 1];
  char password_salt_hex[PASSWORD_SALT_HEX_LENGTH + 1];
  char password_hash_hex[PASSWORD_HASH_HEX_LENGTH + 1];
  UserRole role;
  /// 卡用户须关联已开卡学号；其他角色为空字符串。
  char student_id[STUDENT_ID_LENGTH + 1];
} Account;

#endif // CAMP_CARD_MODELS_H
