#ifndef CAMP_CARD_CLI_H
#define CAMP_CARD_CLI_H

#include "common.h"
#include <stdint.h>

/**
 * @brief 运行校园卡命令行交互。
 * @details 调用前须由 `main` 成功调用 `storage_init`，返回后由 `main` 调用
 *          `storage_shutdown`。只通过业务服务接口执行操作，不直接调用存储接口或读写 JSON。
 *          通过 `auth_needs_bootstrap` 查询初始化状态；仅在需要时调用
 *          `auth_bootstrap_admin` 创建管理员。三角色菜单按权限路由，退出登录时清除本地会话。
 *          使用 UTF-8 输入输出，并在 Windows 控制台设置代码页。遇到 `EOF` 时正常退出；
 *          超长输入须排空当前行后提示。密码在终端输入时关闭回显，且不得主动打印或记录。
 *          业务错误提示后返回对应菜单，不输出路径、凭据或内部状态。输入输出流故障返回
 *          `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @return `APP_OK` 表示正常退出（包括 `EOF`）；输入输出流故障返回 `APP_ERR_IO`，分配失败返回
 *         `APP_ERR_NO_MEMORY`。
 */
AppResult cli_run();
/**
 * @brief 将金额文本解析为整数分。
 * @details 仅接受 ASCII 数字和可选的小数点，至少一位整数。存在小数点时，小数点后必须有
 *          1 到 2 位数字。不接受空白、正负号、指数符号或逗号。整数文本按元换算为分，
 *          例如 `1` 表示 `100` 分；小数文本最多两位小数。接受零，零金额的充值与消费由
 *          业务层拒绝。解析不使用浮点数；失败时 `out_cents` 保持原值。
 * @param[in] text 待解析的金额文本；只接受规定格式的 ASCII 数字金额。
 * @param[out] out_cents 成功时接收整数分金额；失败时保持原值。
 * @return `APP_OK` 表示解析成功；格式错误、参数无效或数值超出 `int64_t` 范围时返回
 *         `APP_ERR_INVALID_ARGUMENT`。
 */
AppResult cli_parse_amount(const char *text, int64_t *out_cents);

#endif // CAMP_CARD_CLI_H
