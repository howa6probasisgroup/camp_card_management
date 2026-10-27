#ifndef CAMP_CARD_CLI_H
#define CAMP_CARD_CLI_H

#include "common.h"
#include <stdint.h>

/// 调用前 storage_init 成功，返回后由 main 调用 storage_shutdown。
/// 仅通过 Service 执行业务；CLI 不直接调用 Storage（生命周期由 main 管理）。
/// 首次管理员初始化由 auth_bootstrap_admin 判断，三角色菜单按权限路由。
/// UTF-8 输入输出，Windows 明确设置编码；EOF 正常退出 APP_OK。
/// 超长输入须排空该行再报错；密码可回显，但不得主动打印或记录。
/// 退出登录清除本地会话。业务错误提示后留在菜单，不泄露路径/凭据。
/// 输入/输出流故障 IO，分配失败 NO_MEMORY。
AppResult cli_run(void);
/// 金额解析：只接受 ASCII 数字及可选小数点后 1..2 位数字，
/// 至少一位整数；无空白/正负号/指数/逗号，接受 0，溢出 INVALID_ARGUMENT。
/// 不用浮点数，失败不改变 out_cents；充值/消费另行拒绝零。
AppResult cli_parse_amount(const char *text, int64_t *out_cents);

#endif // CAMP_CARD_CLI_H
