#ifndef CAMP_CARD_SERVICES_AUTH_SERVICE_H
#define CAMP_CARD_SERVICES_AUTH_SERVICE_H

#include "../models.h"

/**
 * @brief 查询是否需要首次初始化；
 * @details 成功时写入是否无账号，失败保持输出原值。 调用前须初始化存储；NULL 返回 `APP_ERR_INVALID_ARGUMENT`，存储错误原样传播。
 * @param[out] out_needed 成功时接收是否需要首次创建管理员的标志；失败时保持原值。
 * @return `APP_OK` 表示查询成功并写入 out_needed；参数无效返回 `APP_ERR_INVALID_ARGUMENT`，存储错误原样传播。
 */
AppResult auth_needs_bootstrap(bool *out_needed);

/**
 * @brief Service 的 actor 来自成功登录，由调用方保存为会话快照，不接受 UI 自造角色。
 * @details 所有业务操作自行检查参数和权限。修改密码后会话继续有效。 登录失败（用户名不存在/密码错误）统一 `APP_ERR_AUTH_FAILED`；存储损坏/`APP_ERR_IO` 则传播。
 *          非法格式的用户名或长度不合法的密码统一返回 `APP_ERR_AUTH_FAILED`； NULL 参数返回
 *          `APP_ERR_INVALID_ARGUMENT`，密码派生内部错误原样传播。 out 为含凭据的内部会话数据，不得整体显示。退出登录由 CLI 清除会话。
 * @param[in] username 用于登录的用户名。
 * @param[in] password 用于登录的明文密码；只在调用期间使用。
 * @param[out] out 成功时接收内部会话账号快照，包含的凭据不得整体显示或记录。
 * @return `APP_OK` 表示登录成功并写入会话快照；用户名或密码错误返回 `APP_ERR_AUTH_FAILED`，参数为空返回 `APP_ERR_INVALID_ARGUMENT`，存储及派生错误按原结果传播。
 */
AppResult auth_login(const char *username, const char *password, Account *out);
/**
 * @brief 仅无任何有效账号时创建首个管理员；
 * @details 已有账号 `APP_ERR_ALREADY_EXISTS`； 损坏账号须报错，不得当作空目录。使用同一密码派生方案。
 * @param[in] username 首次管理员的用户名。
 * @param[in] password 首次管理员的明文密码；只在调用期间使用。
 * @return `APP_OK` 表示首个管理员已创建；已有账号返回 `APP_ERR_ALREADY_EXISTS`，账号数据损坏或存储错误按原结果传播。
 */
AppResult auth_bootstrap_admin(const char *username, const char *password);
/**
 * @brief 重新读取账号校验旧密码；
 * @details 旧密码错误或长度不合法返回 `APP_ERR_AUTH_FAILED`。 NULL 或新密码长度不合法返回
 *          `APP_ERR_INVALID_ARGUMENT`；凭据损坏及内部错误原样传播。 每次生成新盐，成功后当前会话继续有效。
 * @param[in] actor 成功登录后保存的调用者会话快照。
 * @param[in] old_password 当前密码的明文值；只在调用期间使用。
 * @param[in] new_password 要设置的新密码明文值；只在调用期间使用。
 * @return `APP_OK` 表示密码已修改；旧密码错误返回 `APP_ERR_AUTH_FAILED`，参数或新密码长度无效返回 `APP_ERR_INVALID_ARGUMENT`，凭据损坏及内部错误按原结果传播。
 */
AppResult auth_change_password(const Account *actor, const char *old_password,
                               const char *new_password);

#endif // CAMP_CARD_SERVICES_AUTH_SERVICE_H
