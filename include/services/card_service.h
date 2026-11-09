#ifndef CAMP_CARD_SERVICES_CARD_SERVICE_H
#define CAMP_CARD_SERVICES_CARD_SERVICE_H

#include "../data_structures.h"
#include <stdint.h>

/**
 * @brief 每个接口自行校验 actor/角色/学号；
 * @details NULL 参数 `APP_ERR_INVALID_ARGUMENT`， 无权角色或访问他人记录 `APP_ERR_PERMISSION_DENIED`；存储错误原样传播。 仅业务员；重复学号
 *          `APP_ERR_ALREADY_EXISTS`；强制零余额、`CARD_STATUS_ACTIVE`。 先保存主记录再建索引，失败可能已保存主记录，不回滚。
 * @param[in] actor 已登录的调用者会话；必须具有卡业务员角色。
 * @param[in] input 待开卡的学生资料；余额和状态由接口初始化。
 * @return `APP_OK` 表示主记录和姓名索引已处理；重复学号返回 `APP_ERR_ALREADY_EXISTS`，无权限返回 `APP_ERR_PERMISSION_DENIED`，存储错误原样传播。
 */
AppResult card_issue(const Account *actor, const StudentProfileInput *input);
/**
 * @brief 业务员可查任意学号；
 * @details 卡用户只能查本人；管理员无权限。
 * @param[in] actor 已登录的调用者会话；权限决定可查询的学号范围。
 * @param[in] student_id 要查询的学号。
 * @param[out] out 成功时接收学生及校园卡主记录。
 * @return `APP_OK` 表示记录已写入 out；无权访问返回 `APP_ERR_PERMISSION_DENIED`，记录不存在返回 `APP_ERR_NOT_FOUND`。
 */
AppResult card_get_by_id(const Account *actor, const char *student_id,
                         StudentCardRecord *out);
/**
 * @brief 仅业务员；
 * @details 全部精确同名结果，无命中 `APP_OK` 空表；out 初始化为空， 成功调用方 clear，失败保持为空。
 * @param[in] actor 已登录的调用者会话；必须具有卡业务员角色。
 * @param[in] name 要精确匹配的学生姓名。
 * @param[in,out] out 调用前初始化为空的学生列表；成功后由调用方清理。
 * @return `APP_OK` 表示查询完成（无命中时为空表）；无权限返回 `APP_ERR_PERMISSION_DENIED`，失败时 out 保持为空。
 */
AppResult card_find_by_name(const Account *actor, const char *name,
                            StudentList *out);
/**
 * @brief 仅卡用户，修改绑定记录电话，允许空字符串；
 * @details 冻结卡仍可修改电话。
 * @param[in] actor 已登录的卡用户会话；目标卡从其绑定学号确定。
 * @param[in] phone 要设置的联系电话；允许为空字符串。
 * @return `APP_OK` 表示本人卡记录的电话已更新；无权限返回 `APP_ERR_PERMISSION_DENIED`，存储错误原样传播。
 */
AppResult card_update_own_phone(const Account *actor, const char *phone);
/**
 * @brief 仅卡用户，目标学号只取自 actor；
 * @details 正整数分且卡 `CARD_STATUS_ACTIVE`。 冻结 `APP_ERR_CARD_FROZEN`，余额不足 `APP_ERR_INSUFFICIENT_BALANCE`，充值溢出
 *          `APP_ERR_INVALID_ARGUMENT`；上述校验失败不写文件，输出余额只在成功时写入。
 * @param[in] actor 已登录的卡用户会话；目标卡从其绑定学号确定。
 * @param[in] amount_cents 要充值的正整数分金额。
 * @param[out] new_balance_cents 成功时接收充值后的余额；失败时保持原值。
 * @return `APP_OK` 表示充值成功并写入新余额；冻结卡返回 `APP_ERR_CARD_FROZEN`，余额溢出或金额无效返回 `APP_ERR_INVALID_ARGUMENT`。
 */
AppResult card_recharge(const Account *actor, int64_t amount_cents,
                        int64_t *new_balance_cents);
/**
 * @brief 仅卡用户，学号取自 actor；
 * @details 金额须为正整数分且卡正常。冻结返回 `APP_ERR_CARD_FROZEN`，余额不足返回
 *          `APP_ERR_INSUFFICIENT_BALANCE`；校验失败不写文件，成功才写入新余额。
 * @param[in] actor 已登录的卡用户会话；目标卡从其绑定学号确定。
 * @param[in] amount_cents 要消费的正整数分金额。
 * @param[out] new_balance_cents 成功时接收消费后的余额；失败时保持原值。
 * @return `APP_OK` 表示消费成功并写入新余额；冻结卡返回 `APP_ERR_CARD_FROZEN`，余额不足返回 `APP_ERR_INSUFFICIENT_BALANCE`，金额无效返回
 *         `APP_ERR_INVALID_ARGUMENT`。
 */
AppResult card_consume(const Account *actor, int64_t amount_cents,
                       int64_t *new_balance_cents);
/**
 * @brief 仅业务员；
 * @details 仅接受 `CARD_STATUS_ACTIVE`/`CARD_STATUS_FROZEN`；同状态 `APP_OK`，不改变余额。
 * @param[in] actor 已登录的调用者会话；必须具有卡业务员角色。
 * @param[in] student_id 要设置状态的学生学号。
 * @param[in] status 要设置的校园卡状态，仅允许 `CARD_STATUS_ACTIVE` 或 `CARD_STATUS_FROZEN`。
 * @return `APP_OK` 表示状态已设置或原本相同；无权限返回 `APP_ERR_PERMISSION_DENIED`，其他存储错误原样传播。
 */
AppResult card_set_status(const Account *actor, const char *student_id,
                          CardStatus status);

#endif // CAMP_CARD_SERVICES_CARD_SERVICE_H
