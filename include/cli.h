#ifndef CAMP_CARD_CLI_H
#define CAMP_CARD_CLI_H

#include "common.h"
#include <stdint.h>

/**
 * @brief 调用前 `storage_init` 成功，返回后由 main 调用 `storage_shutdown`。
 * @details 仅通过 Service 执行业务；CLI 不直接调用 Storage（生命周期由 main 管理）。 通过 `auth_needs_bootstrap`
 *          查询初始化状态，查询失败不得进入初始化流程。 仅在需要时调用 `auth_bootstrap_admin` 创建管理员，三角色菜单按权限路由。 UTF-8
 *          输入输出，Windows 明确设置编码；EOF 正常退出 `APP_OK`。 超长输入须排空该行再报错；密码可回显，但不得主动打印或记录。
 *          退出登录清除本地会话。业务错误提示后留在菜单，不泄露路径/凭据。 输入/输出流故障 `APP_ERR_IO`，分配失败 `APP_ERR_NO_MEMORY`。
 * @return `APP_OK` 表示正常退出（包括 EOF）；输入输出故障返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult cli_run(void);
/**
 * @brief 金额解析：只接受 ASCII 数字及可选小数点后 1..2 位数字， 至少一位整数；
 * @details 无空白/正负号/指数/逗号，接受 0，溢出 `APP_ERR_INVALID_ARGUMENT`。 整数输入按元换算为分（例如 `1` 表示 100 分），带小数点时最多两位小数。
 *          不用浮点数，失败不改变 out_cents；充值/消费另行拒绝零。
 * @param[in] text 待解析的金额文本；只接受规定格式的 ASCII 数字金额。
 * @param[out] out_cents 成功时接收整数分金额；解析失败时保持原值。
 * @return `APP_OK` 表示解析成功；格式错误、参数无效或数值溢出返回 `APP_ERR_INVALID_ARGUMENT`。
 */
AppResult cli_parse_amount(const char *text, int64_t *out_cents);

#endif // CAMP_CARD_CLI_H
