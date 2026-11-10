#include "cli.h"
#include "storage.h"

#include <stdio.h>
#include <stdlib.h>

// 数据目录相对于启动工作目录。正式数据与测试数据分离：测试由各自临时目录
// 调用存储接口，入口只使用运行目录下的 data。
#ifdef CAMP_CARD_BACKEND_READY
static const char DATA_DIRECTORY[] = "data";

// 入口只负责存储生命周期，业务操作全部由 CLI 通过服务接口完成。
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
#else
// 存储和服务实现尚未接入时，入口不返回成功，也不伪造业务结果。
int main() {
  fputs("存储与服务实现尚未接入，无法启动。\n", stderr);
  return EXIT_FAILURE;
}
#endif
