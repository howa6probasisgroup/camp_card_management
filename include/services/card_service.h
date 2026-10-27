#ifndef CAMP_CARD_SERVICES_CARD_SERVICE_H
#define CAMP_CARD_SERVICES_CARD_SERVICE_H

#include "../data_structures.h"
#include <stdint.h>

/// 每个接口自行校验 actor/角色/学号；NULL 参数 INVALID_ARGUMENT，
/// 无权角色或访问他人记录 PERMISSION_DENIED；存储错误原样传播。
/// 仅业务员；重复学号 ALREADY_EXISTS；强制零余额、ACTIVE。
/// 先保存主记录再建索引，失败可能已保存主记录，不回滚。
AppResult card_issue(const Account *actor, const StudentProfileInput *input);
/// 业务员可查任意学号；卡用户只能查本人；管理员无权限。
AppResult card_get_by_id(const Account *actor, const char *student_id,
                         StudentCardRecord *out);
/// 仅业务员；全部精确同名结果，无命中 APP_OK 空表；out 初始化为空，
/// 成功调用方 clear，失败保持为空。
AppResult card_find_by_name(const Account *actor, const char *name,
                            StudentList *out);
/// 仅卡用户，修改绑定记录电话，允许空字符串；冻结卡仍可修改电话。
AppResult card_update_own_phone(const Account *actor, const char *phone);
/// 仅卡用户，目标学号只取自 actor；正整数分且卡 ACTIVE。
/// 冻结 CARD_FROZEN，余额不足 INSUFFICIENT_BALANCE，充值溢出
/// INVALID_ARGUMENT；上述校验失败不写文件，输出余额只在成功时写入。
AppResult card_recharge(const Account *actor, int64_t amount_cents,
                        int64_t *new_balance_cents);
/// 仅卡用户，学号取自 actor；金额须为正整数分且卡正常。冻结返回
/// APP_ERR_CARD_FROZEN，余额不足返回
/// APP_ERR_INSUFFICIENT_BALANCE；校验失败不写文件，成功才写入新余额。
AppResult card_consume(const Account *actor, int64_t amount_cents,
                       int64_t *new_balance_cents);
/// 仅业务员；仅接受 ACTIVE/FROZEN；同状态 APP_OK，不改变余额。
AppResult card_set_status(const Account *actor, const char *student_id,
                          CardStatus status);

#endif // CAMP_CARD_SERVICES_CARD_SERVICE_H
