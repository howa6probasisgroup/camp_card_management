#include "cli.h"
#include "storage.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief 正式数据目录名。
 * @details 路径相对于启动工作目录；测试通过各自临时目录调用存储接口，入口
 *          只使用该目录。
 */
static constexpr char DATA_DIRECTORY[] = "data";

/**
 * @brief 运行正式程序入口。
 * @details 初始化存储、运行 CLI 并关闭存储；业务操作由 CLI 通过服务接口完成。
 * @return 全部步骤成功时返回 `EXIT_SUCCESS`，否则返回 `EXIT_FAILURE`。
 */
int main() {
  AppResult result = storage_init(DATA_DIRECTORY);
  if (result != APP_OK) {
    fputs("无法初始化数据目录，程序退出。\n", stderr);
    return EXIT_FAILURE;
  }

  result = cli_run();
  const AppResult shutdown_result = storage_shutdown();

  if (result != APP_OK) {
    fputs("程序运行失败，已退出。\n", stderr);
    return EXIT_FAILURE;
  }
  if (shutdown_result != APP_OK) {
    fputs("数据目录关闭失败。\n", stderr);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
