#ifndef CAMP_CARD_SERVICES_AUTH_SERVICE_H
#define CAMP_CARD_SERVICES_AUTH_SERVICE_H

#include "../models.h"

/// 查询是否需要首次初始化；成功时写入是否无账号，失败保持输出原值。
/// 调用前须初始化存储；NULL 返回 APP_ERR_INVALID_ARGUMENT，存储错误原样传播。
AppResult auth_needs_bootstrap(bool *out_needed);

/// Service 的 actor 来自成功登录，由调用方保存为会话快照，不接受 UI 自造角色。
/// 所有业务操作自行检查参数和权限。修改密码后会话继续有效。
/// 登录失败（用户名不存在/密码错误）统一 AUTH_FAILED；存储损坏/IO 则传播。
/// 非法格式的用户名或长度不合法的密码统一返回 APP_ERR_AUTH_FAILED；
/// NULL 参数返回 APP_ERR_INVALID_ARGUMENT，密码派生内部错误原样传播。
/// out 为含凭据的内部会话数据，不得整体显示。退出登录由 CLI 清除会话。
AppResult auth_login(const char *username, const char *password, Account *out);
/// 仅无任何有效账号时创建首个管理员；已有账号 ALREADY_EXISTS；
/// 损坏账号须报错，不得当作空目录。使用同一密码派生方案。
AppResult auth_bootstrap_admin(const char *username, const char *password);
/// 重新读取账号校验旧密码；旧密码错误或长度不合法返回 APP_ERR_AUTH_FAILED。
/// NULL 或新密码长度不合法返回
/// APP_ERR_INVALID_ARGUMENT；凭据损坏及内部错误原样传播。
/// 每次生成新盐，成功后当前会话继续有效。
AppResult auth_change_password(const Account *actor, const char *old_password,
                               const char *new_password);

#endif // CAMP_CARD_SERVICES_AUTH_SERVICE_H
