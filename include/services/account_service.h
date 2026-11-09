#ifndef CAMP_CARD_SERVICES_ACCOUNT_SERVICE_H
#define CAMP_CARD_SERVICES_ACCOUNT_SERVICE_H

#include "../data_structures.h"

/**
 * @brief 仅管理员；
 * @details 创建前查重，重复返回
 * `APP_ERR_ALREADY_EXISTS`，不覆盖。卡用户须关联已开卡学号，否则
 * `APP_ERR_NOT_FOUND`；其他角色 student_id 须为空字符串。
 * @param[in] actor 已登录的调用者会话；必须具有管理员角色。
 * @param[in] username 要创建的用户名。
 * @param[in] password 新账号的明文密码；只在调用期间使用。
 * @param[in] role 新账号的用户角色。
 * @param[in] student_id 卡用户关联的已开卡学号；其他角色须传空字符串。
 * @return `APP_OK` 表示账号已创建；重复用户名返回
 * `APP_ERR_ALREADY_EXISTS`，关联学生不存在返回 `APP_ERR_NOT_FOUND`，无权限返回
 *         `APP_ERR_PERMISSION_DENIED`。
 */
AppResult account_create(const Account *actor, const char *username,
                         const char *password, UserRole role,
                         const char *student_id);
/**
 * @brief 仅管理员；
 * @details 读取账号，成功写入 out。账号包含凭据，CLI 仅显示用户名、角色和学号。
 * @param[in] actor 已登录的调用者会话；必须具有管理员角色。
 * @param[in] username 要查询的账号用户名。
 * @param[out] out 成功时接收账号数据，其中包含的凭据仅供程序内部使用。
 * @return `APP_OK` 表示账号已写入 out；账号不存在返回
 * `APP_ERR_NOT_FOUND`，无权限返回 `APP_ERR_PERMISSION_DENIED`。
 */
AppResult account_get(const Account *actor, const char *username, Account *out);
/**
 * @brief 仅管理员；
 * @details out 须初始化为空，成功后由调用方
 * `account_list_clear`，失败保持为空；顺序不保证，不得输出凭据。
 * @param[in] actor 已登录的调用者会话；必须具有管理员角色。
 * @param[in,out] out 调用前初始化为空的账号列表；成功后由调用方调用
 * `account_list_clear` 释放。
 * @return `APP_OK` 表示列表已构造；无权限返回
 * `APP_ERR_PERMISSION_DENIED`，失败时列表保持为空。
 */
AppResult account_list(const Account *actor, AccountList *out);
/**
 * @brief 仅管理员；
 * @details 不能删除 actor
 * 的用户名（`APP_ERR_PERMISSION_DENIED`）；仅删除账号，不删卡记录。
 * 不额外保护最后一个管理员。
 * @param[in] actor 已登录的调用者会话；必须具有管理员角色。
 * @param[in] username 要删除的账号用户名；不得是调用者自己的用户名。
 * @return `APP_OK` 表示账号已删除；账号不存在返回
 * `APP_ERR_NOT_FOUND`，删除自身账号返回 `APP_ERR_PERMISSION_DENIED`。
 */
AppResult account_delete(const Account *actor, const char *username);

#endif // CAMP_CARD_SERVICES_ACCOUNT_SERVICE_H
