#ifndef CAMP_CARD_SERVICES_ACCOUNT_SERVICE_H
#define CAMP_CARD_SERVICES_ACCOUNT_SERVICE_H

#include "../data_structures.h"

/// 仅管理员；创建前查重，重复返回
/// APP_ERR_ALREADY_EXISTS，不覆盖。卡用户须关联已开卡学号，否则
/// APP_ERR_NOT_FOUND；其他角色 student_id 须为空字符串。
AppResult account_create(const Account *actor, const char *username,
                         const char *password, UserRole role,
                         const char *student_id);
/// 仅管理员；读取账号，成功写入 out。账号包含凭据，CLI
/// 仅显示用户名、角色和学号。
AppResult account_get(const Account *actor, const char *username, Account *out);
/// 仅管理员；out 须初始化为空，成功后由调用方
/// account_list_clear，失败保持为空；顺序不保证，不得输出凭据。
AppResult account_list(const Account *actor, AccountList *out);
/// 仅管理员；不能删除 actor
/// 的用户名（PERMISSION_DENIED）；仅删除账号，不删卡记录。
/// 不额外保护最后一个管理员。
AppResult account_delete(const Account *actor, const char *username);

#endif // CAMP_CARD_SERVICES_ACCOUNT_SERVICE_H
