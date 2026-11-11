#include "cli.h"

#include "services/account_service.h"
#include "services/auth_service.h"
#include "services/card_service.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// CLI 模块负责命令行输入输出、三角色菜单和程序会话串联。菜单只收集输入、
// 调用业务服务并展示结果；权限、余额和卡状态规则由服务层执行。CLI 不直接
// 调用存储函数、不读写 JSON，错误提示只依据返回码生成，避免泄露内部路径、
// 密码盐、派生值或堆栈。
//
// 金额始终使用整数分：解析时不引入浮点数，展示固定两位小数。业务层负责
// 拒绝零金额的充值与消费；本模块只负责把用户文本正确解析为分值并检查溢出。

// 单行输入的最大缓冲区容量（字节）。足够容纳用户名、密码、姓名和金额等
// 正常输入；超长输入由 cli_read_line 排空该行后提示，剩余字符不会被当作
// 下一次选择。
enum { CLI_LINE_CAPACITY = 4096 };

// 行输入内部结构：记录是否遇到 EOF、是否超长以及是否发生流故障，供调用方
// 区分正常空行、超长行和输入流故障。
typedef struct {
  char buffer[CLI_LINE_CAPACITY];
  size_t length;
  bool too_long;
  bool eof;
  bool io_error;
} CliLine;

// 会话级输入状态。任一次读取遇到 EOF 或流故障后，菜单不再继续询问。
typedef struct {
  bool eof;
  bool io_error;
} CliInput;

// 单次输入结果，区分正常、EOF、超长和流故障。EOF 不是错误。
typedef enum {
  CLI_INPUT_OK = 0,
  CLI_INPUT_EOF,
  CLI_INPUT_TOO_LONG,
  CLI_INPUT_IO
} CliInputStatus;

// 金额和菜单编号只接受 ASCII 数字，显式比较而不依赖当前 locale 的 isdigit。
static bool is_ascii_digit(const char value) {
  return value >= '0' && value <= '9';
}

// 清除密码等敏感缓冲区，使用 volatile 防止编译器把清零优化掉。
static void secure_clear(void *data, size_t size) {
  volatile unsigned char *cursor = data;
  while (size > 0) {
    *cursor++ = 0;
    size--;
  }
}

// 清空 stdin 当前行的剩余字符（含换行），避免超长输入的残留字符被当作
// 下一次选择。排空过程中遇到 EOF 或流故障时记录状态。
static void drain_current_line(CliLine *line) {
  int value;
  do {
    value = getchar();
  } while (value != '\n' && value != EOF);

  if (value == EOF) {
    line->eof = true;
    if (ferror(stdin)) {
      line->io_error = true;
    }
  }
}

// 读取整行输入到内部缓冲区。自动去除末尾换行符与行尾回车符，保留 UTF-8
// 原始字节不做编码转换。EOF 前若有未换行的内容，按一行正常处理。
// 返回 false 表示没有可用输入（空输入遇到 EOF 或流故障）；此时调用方应
// 停止读取，不再把残留内容当成选项。
static bool cli_read_line(CliLine *line) {
  line->length = 0;
  line->too_long = false;
  line->eof = false;
  line->io_error = false;

  int value = 0;
  while (line->length + 1 < sizeof line->buffer) {
    value = getchar();
    if (value == '\n' || value == EOF) {
      break;
    }
    line->buffer[line->length++] = (char)value;
  }

  if (value != '\n' && value != EOF) {
    // 缓冲区已满且尚未读到行尾，说明输入超长，排空当前行。
    line->too_long = true;
    drain_current_line(line);
  } else if (value == EOF) {
    line->eof = true;
    if (ferror(stdin)) {
      line->io_error = true;
    }
  }

  line->buffer[line->length] = '\0';
  if (line->length > 0 && line->buffer[line->length - 1] == '\r') {
    line->buffer[--line->length] = '\0';
  }

  if (line->io_error) {
    return false;
  }
  if (line->eof && line->length == 0 && !line->too_long) {
    return false;
  }
  return true;
}

// Windows 控制台在读取输入前，将控制台代码页设置为 UTF-8，使 UTF-8 输入
// 输出与文件存储保持一致。仅在 Windows 平台调用，macOS/POSIX 无此需求。
static void configure_console_encoding() {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#else
  (void)0;
#endif
}

AppResult cli_parse_amount(const char *text, int64_t *out_cents) {
  if (text == nullptr || out_cents == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  // 只接受非负十进制整数，或最多两位小数的十进制数。不允许正负号、指数、
  // 逗号、空白或多余字符。输入必须至少有一位整数，可选的 "." 之后须有
  // 1..2 位数字。允许解析零，由业务层拒绝零金额充值与消费。
  const char *cursor = text;
  if (*cursor == '\0' || !is_ascii_digit(*cursor)) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  // 整数部分：累积到分值，检查溢出。用 uint64 累积以覆盖 int64 全范围。
  uint64_t cents = 0;
  while (is_ascii_digit(*cursor)) {
    const unsigned digit = (unsigned)(*cursor - '0');
    if (cents > (UINT64_MAX - digit) / 10) {
      return APP_ERR_INVALID_ARGUMENT; // 转换溢出
    }
    cents = cents * 10 + digit;
    cursor++;
  }

  // 小数部分：最多两位，换算为分。没有小数点时整数部分按元换算为分。
  if (*cursor == '.') {
    cursor++;
    unsigned fractional_digits = 0;
    uint64_t fraction = 0;
    while (is_ascii_digit(*cursor) && fractional_digits < 2) {
      const unsigned digit = (unsigned)(*cursor - '0');
      if (fraction > (UINT64_MAX - digit) / 10) {
        return APP_ERR_INVALID_ARGUMENT;
      }
      fraction = fraction * 10 + digit;
      fractional_digits++;
      cursor++;
    }
    if (fractional_digits == 0) {
      return APP_ERR_INVALID_ARGUMENT; // "." 之后必须紧跟数字
    }
    // 多于两位小数：非法格式。
    if (is_ascii_digit(*cursor)) {
      return APP_ERR_INVALID_ARGUMENT;
    }
    if (fractional_digits == 1) {
      fraction *= 10; // 一位小数视为 X0 分
    }
    if (cents > (UINT64_MAX - fraction) / 100) {
      return APP_ERR_INVALID_ARGUMENT;
    }
    cents = cents * 100 + fraction;
  } else {
    // 整数输入表示元，例如 "1" 表示 1.00 元、即 100 分。
    if (cents > UINT64_MAX / 100) {
      return APP_ERR_INVALID_ARGUMENT;
    }
    cents *= 100;
  }

  // 之后不允许出现任何字符（空白、符号、指数、逗号等均非法）。
  if (*cursor != '\0') {
    return APP_ERR_INVALID_ARGUMENT;
  }

  // 分值不超过 int64 上界才写出；失败不改变 out_cents。
  if (cents > (uint64_t)INT64_MAX) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  *out_cents = (int64_t)cents;
  return APP_OK;
}

// 把整数分格式化为固定两位小数的元。返回值只表示缓冲区是否足够；余额由
// 存储层保证非负，调用方不需要处理负数的业务含义。
static bool format_amount(const int64_t cents, char *out,
                          const size_t capacity) {
  if (out == nullptr || capacity == 0 || cents < 0) {
    return false;
  }
  const int written = snprintf(out, capacity, "%" PRId64 ".%02d", cents / 100,
                               (int)(cents % 100));
  return written >= 0 && (size_t)written < capacity;
}

// 根据返回码生成用户可见提示。不得在这里附加路径、凭据或内部状态。
static void print_error(const AppResult result) {
  const char *message;
  switch (result) {
  case APP_OK:
    message = nullptr;
    break;
  case APP_ERR_INVALID_ARGUMENT:
    message = "输入不合法";
    break;
  case APP_ERR_NOT_FOUND:
    message = "记录不存在";
    break;
  case APP_ERR_ALREADY_EXISTS:
    message = "记录已存在";
    break;
  case APP_ERR_AUTH_FAILED:
    message = "用户名或密码错误";
    break;
  case APP_ERR_PERMISSION_DENIED:
    message = "没有执行该操作的权限";
    break;
  case APP_ERR_INSUFFICIENT_BALANCE:
    message = "余额不足";
    break;
  case APP_ERR_CARD_FROZEN:
    message = "校园卡已冻结";
    break;
  case APP_ERR_IO:
    message = "读写失败";
    break;
  case APP_ERR_INVALID_DATA:
    message = "数据损坏";
    break;
  case APP_ERR_NO_MEMORY:
    message = "内存不足";
    break;
  default:
    message = "未知错误";
    break;
  }
  if (message != nullptr) {
    printf("操作失败：%s。\n", message);
  }
}

// 读写失败、数据损坏和内存不足表示底层状态已不可继续使用；认证失败和业务
// 校验失败只提示并留在菜单。
static bool result_is_fatal(const AppResult result) {
  return result == APP_ERR_IO || result == APP_ERR_INVALID_DATA ||
         result == APP_ERR_NO_MEMORY;
}

static const char *role_text(const UserRole role) {
  switch (role) {
  case ROLE_ADMIN:
    return "管理员";
  case ROLE_CARD_USER:
    return "卡用户";
  case ROLE_CARD_CLERK:
    return "卡业务员";
  default:
    return "未知角色";
  }
}

static const char *status_text(const CardStatus status) {
  switch (status) {
  case CARD_STATUS_ACTIVE:
    return "正常";
  case CARD_STATUS_FROZEN:
    return "冻结";
  default:
    return "未知状态";
  }
}

// 展示账号摘要。只显示用户名、角色和关联学号；盐与派生值不得输出。
static void print_account(const Account *account) {
  printf("用户名：%s\n", account->username);
  printf("角色：%s\n", role_text(account->role));
  if (account->role == ROLE_CARD_USER) {
    printf("关联学号：%s\n", account->student_id);
  } else {
    printf("关联学号：无\n");
  }
}

// 展示校园卡记录。余额由整数分格式化，不使用浮点数。
static void print_card(const StudentCardRecord *record) {
  char balance[32];
  if (!format_amount(record->balance_cents, balance, sizeof balance)) {
    (void)snprintf(balance, sizeof balance, "未知");
  }
  printf("学号：%s\n", record->student_id);
  printf("姓名：%s\n", record->name);
  printf("性别：%s\n", record->sex);
  printf("院系：%s\n", record->department);
  printf("班级：%s\n", record->class_name);
  printf("电话：%s\n", record->phone[0] == '\0' ? "（未填写）" : record->phone);
  printf("余额：%s 元\n", balance);
  printf("状态：%s\n", status_text(record->status));
}

// 提示一行输入。超长输入已被排空，提示后由调用方返回菜单；EOF 与流故障
// 只记录在会话状态中，由最外层流程决定退出。
static CliInputStatus prompt_line(CliInput *input, const char *label, char *out,
                                  const size_t capacity) {
  printf("%s", label);
  if (fflush(stdout) != 0) {
    input->io_error = true;
    return CLI_INPUT_IO;
  }

  CliLine line;
  if (!cli_read_line(&line)) {
    if (line.io_error) {
      input->io_error = true;
      return CLI_INPUT_IO;
    }
    input->eof = true;
    return CLI_INPUT_EOF;
  }
  if (line.eof) {
    input->eof = true;
  }
  if (line.io_error) {
    input->io_error = true;
    secure_clear(line.buffer, sizeof line.buffer);
    return CLI_INPUT_IO;
  }

  CliInputStatus status = CLI_INPUT_OK;
  if (line.too_long || line.length >= capacity) {
    printf("输入过长，已忽略该行，请重新输入。\n");
    status = CLI_INPUT_TOO_LONG;
  } else {
    memcpy(out, line.buffer, line.length + 1);
  }
  secure_clear(line.buffer, sizeof line.buffer);
  return status;
}

// 读取单数字菜单编号。空行、多个字符和非数字都视为无效选择，返回 OK 并
// 让调用方重新显示菜单。
static CliInputStatus prompt_choice(CliInput *input, const char *label,
                                    int *choice) {
  char text[8] = {0};
  const CliInputStatus status = prompt_line(input, label, text, sizeof text);
  if (status != CLI_INPUT_OK) {
    return status;
  }
  if (text[0] == '\0' || text[1] != '\0' || !is_ascii_digit(text[0])) {
    printf("无效选择，请输入菜单编号。\n");
    *choice = -1;
    return CLI_INPUT_OK;
  }
  *choice = text[0] - '0';
  return CLI_INPUT_OK;
}

// 首轮管理员初始化。输入不合法时留在初始化流程重试；已有账号（并发创建）
// 时提示直接登录。EOF 或流故障由调用方根据 CliInput 状态处理。
static AppResult run_bootstrap(CliInput *input) {
  printf("\n==== 首次启动：创建管理员 ====\n");
  printf("用户名 1-%d 字节；密码 %d-%d 字节。\n", USERNAME_MAX_BYTES,
         PASSWORD_MIN_BYTES, PASSWORD_MAX_BYTES);

  char username[USERNAME_MAX_BYTES + 1] = {0};
  char password[PASSWORD_MAX_BYTES + 1];
  char confirm[PASSWORD_MAX_BYTES + 1];
  AppResult result;

  for (;;) {
    secure_clear(password, sizeof password);
    secure_clear(confirm, sizeof confirm);

    CliInputStatus status =
        prompt_line(input, "管理员用户名：", username, sizeof username);
    if (status == CLI_INPUT_OK) {
      status = prompt_line(input, "密码：", password, sizeof password);
    }
    if (status == CLI_INPUT_OK) {
      status = prompt_line(input, "确认密码：", confirm, sizeof confirm);
    }
    if (status == CLI_INPUT_TOO_LONG && !input->eof) {
      continue;
    }
    if (status != CLI_INPUT_OK) {
      result = status == CLI_INPUT_IO ? APP_ERR_IO : APP_OK;
      break;
    }

    if (strcmp(password, confirm) != 0) {
      printf("两次输入的密码不一致，请重新输入。\n");
      continue;
    }

    result = auth_bootstrap_admin(username, password);
    if (result == APP_OK) {
      printf("管理员账号创建成功，请登录。\n");
      break;
    }
    if (result == APP_ERR_ALREADY_EXISTS) {
      printf("系统中已有账号，请直接登录。\n");
      result = APP_OK;
      break;
    }
    print_error(result);
    if (result_is_fatal(result)) {
      break;
    }
  }

  secure_clear(password, sizeof password);
  secure_clear(confirm, sizeof confirm);
  return result;
}

// 登录。认证失败只提示并回到未登录菜单；存储损坏等致命错误向上传播。
static AppResult run_login(CliInput *input, Account *session, bool *logged_in) {
  *logged_in = false;
  char username[USERNAME_MAX_BYTES + 1] = {0};
  char password[PASSWORD_MAX_BYTES + 1] = {0};

  CliInputStatus status =
      prompt_line(input, "用户名：", username, sizeof username);
  if (status != CLI_INPUT_OK) {
    return status == CLI_INPUT_IO ? APP_ERR_IO : APP_OK;
  }
  status = prompt_line(input, "密码：", password, sizeof password);
  if (status != CLI_INPUT_OK) {
    secure_clear(password, sizeof password);
    return status == CLI_INPUT_IO ? APP_ERR_IO : APP_OK;
  }

  const AppResult result = auth_login(username, password, session);
  secure_clear(password, sizeof password);

  if (result == APP_OK) {
    *logged_in = true;
    printf("登录成功：%s（%s）。\n", session->username,
           role_text(session->role));
    return APP_OK;
  }

  print_error(result);
  if (result_is_fatal(result)) {
    return result;
  }
  return APP_OK;
}

// 以下处理函数返回 APP_OK 表示已处理完（成功或可重试的业务错误），返回
// 致命错误时由角色菜单结束流程。输入遇到 EOF 时由调用方检查会话状态。
#define ASK_OR_FINISH(input, label, buffer)                                    \
  do {                                                                         \
    const CliInputStatus ask_status_ =                                         \
        prompt_line((input), (label), (buffer), sizeof(buffer));               \
    if (ask_status_ != CLI_INPUT_OK) {                                         \
      result = ask_status_ == CLI_INPUT_IO ? APP_ERR_IO : APP_OK;              \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0)

static AppResult handle_admin_create(CliInput *input, const Account *actor) {
  char username[USERNAME_MAX_BYTES + 1] = {0};
  char password[PASSWORD_MAX_BYTES + 1] = {0};
  char confirm[PASSWORD_MAX_BYTES + 1] = {0};
  char student_id[STUDENT_ID_LENGTH + 1] = {0};
  int role_choice = -1;
  AppResult result = APP_OK;

  ASK_OR_FINISH(input, "新账号用户名：", username);
  ASK_OR_FINISH(input, "新账号密码：", password);
  ASK_OR_FINISH(input, "确认密码：", confirm);
  if (strcmp(password, confirm) != 0) {
    printf("两次输入的密码不一致，请重新操作。\n");
    goto cleanup;
  }

  const CliInputStatus status = prompt_choice(
      input, "角色（1 管理员，2 卡用户，3 卡业务员）：", &role_choice);
  if (status != CLI_INPUT_OK) {
    result = status == CLI_INPUT_IO ? APP_ERR_IO : APP_OK;
    goto cleanup;
  }
  UserRole role;
  switch (role_choice) {
  case 1:
    role = ROLE_ADMIN;
    break;
  case 2:
    role = ROLE_CARD_USER;
    break;
  case 3:
    role = ROLE_CARD_CLERK;
    break;
  default:
    printf("无效角色，请重新操作。\n");
    goto cleanup;
  }

  if (role == ROLE_CARD_USER) {
    ASK_OR_FINISH(input, "关联学号：", student_id);
  }

  result = account_create(actor, username, password, role, student_id);
  if (result == APP_OK) {
    printf("账号创建成功：%s（%s）。\n", username, role_text(role));
  } else {
    print_error(result);
  }

cleanup:
  secure_clear(password, sizeof password);
  secure_clear(confirm, sizeof confirm);
  return result;
}

static AppResult handle_admin_get(CliInput *input, const Account *actor) {
  char username[USERNAME_MAX_BYTES + 1] = {0};
  Account fetched = {0};
  AppResult result;

  ASK_OR_FINISH(input, "要查询的用户名：", username);
  result = account_get(actor, username, &fetched);
  if (result == APP_OK) {
    print_account(&fetched);
  } else {
    print_error(result);
  }

cleanup:
  secure_clear(&fetched, sizeof fetched);
  return result;
}

static AppResult handle_admin_list(const Account *actor) {
  AccountList list;
  account_list_init(&list);

  const AppResult result = account_list(actor, &list);
  if (result == APP_OK) {
    if (list.size == 0) {
      printf("暂无账号。\n");
    } else {
      printf("共 %zu 个账号：\n", list.size);
      for (const AccountNode *node = list.head; node != nullptr;
           node = node->next) {
        printf("--------------------\n");
        print_account(&node->value);
      }
    }
  } else {
    print_error(result);
  }

  account_list_clear(&list);
  return result;
}

static AppResult handle_admin_delete(CliInput *input, const Account *actor) {
  char username[USERNAME_MAX_BYTES + 1] = {0};
  AppResult result;

  ASK_OR_FINISH(input, "要删除的用户名：", username);
  result = account_delete(actor, username);
  if (result == APP_OK) {
    printf("账号已删除：%s。\n", username);
  } else {
    print_error(result);
  }

cleanup:
  return result;
}

static AppResult handle_change_password(CliInput *input, const Account *actor) {
  char old_password[PASSWORD_MAX_BYTES + 1] = {0};
  char new_password[PASSWORD_MAX_BYTES + 1] = {0};
  char confirm[PASSWORD_MAX_BYTES + 1] = {0};
  AppResult result = APP_OK;

  ASK_OR_FINISH(input, "旧密码：", old_password);
  ASK_OR_FINISH(input, "新密码：", new_password);
  ASK_OR_FINISH(input, "确认新密码：", confirm);
  if (strcmp(new_password, confirm) != 0) {
    printf("两次输入的新密码不一致，请重新操作。\n");
    goto cleanup;
  }

  result = auth_change_password(actor, old_password, new_password);
  if (result == APP_OK) {
    printf("密码修改成功，当前会话继续有效。\n");
  } else if (result == APP_ERR_AUTH_FAILED) {
    printf("旧密码错误。\n");
  } else {
    print_error(result);
  }

cleanup:
  secure_clear(old_password, sizeof old_password);
  secure_clear(new_password, sizeof new_password);
  secure_clear(confirm, sizeof confirm);
  return result;
}

static AppResult handle_card_self(const Account *actor) {
  StudentCardRecord record = {0};

  const AppResult result = card_get_by_id(actor, actor->student_id, &record);
  if (result == APP_OK) {
    print_card(&record);
  } else {
    print_error(result);
  }

  secure_clear(&record, sizeof record);
  return result;
}

static AppResult handle_card_phone(CliInput *input, const Account *actor) {
  char phone[PHONE_MAX_BYTES + 1] = {0};
  AppResult result;

  ASK_OR_FINISH(input, "新电话（可留空）：", phone);
  result = card_update_own_phone(actor, phone);
  if (result == APP_OK) {
    printf("联系电话已更新。\n");
  } else {
    print_error(result);
  }

cleanup:
  return result;
}

// 充值或消费。金额先由 CLI 解析为整数分，零金额仍交给服务层拒绝。
static AppResult handle_card_amount(CliInput *input, const Account *actor,
                                    const bool recharge) {
  char text[64] = {0};
  AppResult result;

  ASK_OR_FINISH(input, recharge ? "充值金额（元）：" : "消费金额（元）：",
                text);
  int64_t amount_cents = 0;
  result = cli_parse_amount(text, &amount_cents);
  secure_clear(text, sizeof text);
  if (result != APP_OK) {
    printf("金额格式不正确，请输入非负十进制数，最多两位小数。\n");
    return APP_OK;
  }

  int64_t new_balance_cents = 0;
  if (recharge) {
    result = card_recharge(actor, amount_cents, &new_balance_cents);
  } else {
    result = card_consume(actor, amount_cents, &new_balance_cents);
  }

  if (result == APP_OK) {
    char balance[32];
    if (!format_amount(new_balance_cents, balance, sizeof balance)) {
      (void)snprintf(balance, sizeof balance, "未知");
    }
    printf("%s成功，当前余额：%s 元。\n", recharge ? "充值" : "消费", balance);
  } else {
    print_error(result);
  }

cleanup:
  return result;
}

static AppResult handle_clerk_issue(CliInput *input, const Account *actor) {
  StudentProfileInput profile = {0};
  AppResult result;

  ASK_OR_FINISH(input, "学号：", profile.student_id);
  ASK_OR_FINISH(input, "姓名：", profile.name);
  ASK_OR_FINISH(input, "性别（男/女/其他）：", profile.sex);
  ASK_OR_FINISH(input, "院系：", profile.department);
  ASK_OR_FINISH(input, "班级：", profile.class_name);
  ASK_OR_FINISH(input, "电话（可留空）：", profile.phone);

  result = card_issue(actor, &profile);
  if (result == APP_OK) {
    printf("开卡成功：%s（%s）。\n", profile.name, profile.student_id);
  } else {
    print_error(result);
  }

cleanup:
  secure_clear(&profile, sizeof profile);
  return result;
}

static AppResult handle_clerk_get_by_id(CliInput *input, const Account *actor) {
  char student_id[STUDENT_ID_LENGTH + 1] = {0};
  StudentCardRecord record = {0};
  AppResult result;

  ASK_OR_FINISH(input, "学号：", student_id);
  result = card_get_by_id(actor, student_id, &record);
  if (result == APP_OK) {
    print_card(&record);
  } else {
    print_error(result);
  }

cleanup:
  secure_clear(&record, sizeof record);
  return result;
}

static AppResult handle_clerk_get_by_name(CliInput *input,
                                          const Account *actor) {
  char name[NAME_MAX_BYTES + 1] = {0};
  StudentList list;
  student_list_init(&list);
  AppResult result;

  ASK_OR_FINISH(input, "姓名：", name);
  result = card_find_by_name(actor, name, &list);
  if (result != APP_OK) {
    print_error(result);
    goto cleanup;
  }

  if (list.size == 0) {
    printf("未找到姓名完全匹配的校园卡记录。\n");
  } else {
    printf("共找到 %zu 条同名记录：\n", list.size);
    size_t index = 1;
    for (const StudentNode *node = list.head; node != nullptr;
         node = node->next, index++) {
      printf("---------- 第 %zu 条 ----------\n", index);
      print_card(&node->value);
    }
  }

cleanup:
  student_list_clear(&list);
  return result;
}

static AppResult handle_clerk_status(CliInput *input, const Account *actor,
                                     const CardStatus status) {
  char student_id[STUDENT_ID_LENGTH + 1] = {0};
  AppResult result;

  ASK_OR_FINISH(input, "学号：", student_id);
  result = card_set_status(actor, student_id, status);
  if (result == APP_OK) {
    printf("操作成功：学号 %s 的校园卡已%s。\n", student_id,
           status == CARD_STATUS_FROZEN ? "冻结" : "解冻");
  } else {
    print_error(result);
  }

cleanup:
  return result;
}

static void print_admin_menu(const Account *actor) {
  printf("\n==== 管理员菜单 ====\n");
  printf("当前用户：%s\n", actor->username);
  printf("1. 创建账号\n");
  printf("2. 查询账号\n");
  printf("3. 列出账号\n");
  printf("4. 删除账号\n");
  printf("5. 修改本人密码\n");
  printf("0. 退出登录\n");
}

static void print_card_user_menu(const Account *actor) {
  printf("\n==== 卡用户菜单 ====\n");
  printf("当前用户：%s（学号 %s）\n", actor->username, actor->student_id);
  printf("1. 查询本人卡与余额\n");
  printf("2. 修改联系电话\n");
  printf("3. 充值\n");
  printf("4. 消费\n");
  printf("5. 修改本人密码\n");
  printf("0. 退出登录\n");
}

static void print_clerk_menu(const Account *actor) {
  printf("\n==== 卡业务员菜单 ====\n");
  printf("当前用户：%s\n", actor->username);
  printf("1. 开卡\n");
  printf("2. 按学号查询\n");
  printf("3. 按姓名查询\n");
  printf("4. 冻结卡\n");
  printf("5. 解冻卡\n");
  printf("6. 修改本人密码\n");
  printf("0. 退出登录\n");
}

// 按角色分发菜单编号。返回业务结果供角色菜单判断是否为致命错误；所有
// 提示已由处理函数输出。
static AppResult dispatch_menu(CliInput *input, const Account *actor,
                               const int choice) {
  switch (actor->role) {
  case ROLE_ADMIN:
    switch (choice) {
    case 1:
      return handle_admin_create(input, actor);
    case 2:
      return handle_admin_get(input, actor);
    case 3:
      return handle_admin_list(actor);
    case 4:
      return handle_admin_delete(input, actor);
    case 5:
      return handle_change_password(input, actor);
    default:
      break;
    }
    break;
  case ROLE_CARD_USER:
    switch (choice) {
    case 1:
      return handle_card_self(actor);
    case 2:
      return handle_card_phone(input, actor);
    case 3:
      return handle_card_amount(input, actor, true);
    case 4:
      return handle_card_amount(input, actor, false);
    case 5:
      return handle_change_password(input, actor);
    default:
      break;
    }
    break;
  case ROLE_CARD_CLERK:
    switch (choice) {
    case 1:
      return handle_clerk_issue(input, actor);
    case 2:
      return handle_clerk_get_by_id(input, actor);
    case 3:
      return handle_clerk_get_by_name(input, actor);
    case 4:
      return handle_clerk_status(input, actor, CARD_STATUS_FROZEN);
    case 5:
      return handle_clerk_status(input, actor, CARD_STATUS_ACTIVE);
    case 6:
      return handle_change_password(input, actor);
    default:
      break;
    }
    break;
  default:
    printf("会话角色无效，已退出登录。\n");
    return APP_ERR_INVALID_DATA;
  }

  printf("无效选择，请输入菜单编号。\n");
  return APP_OK;
}

// 登录后的角色菜单。退出登录返回 APP_OK，由外层回到未登录菜单；致命错误
// 向上传播。EOF 由调用方根据 CliInput 状态结束程序。
static AppResult run_role_menu(CliInput *input, const Account *actor) {
  for (;;) {
    if (input->io_error) {
      return APP_ERR_IO;
    }
    if (input->eof) {
      return APP_OK;
    }

    switch (actor->role) {
    case ROLE_ADMIN:
      print_admin_menu(actor);
      break;
    case ROLE_CARD_USER:
      print_card_user_menu(actor);
      break;
    case ROLE_CARD_CLERK:
      print_clerk_menu(actor);
      break;
    default:
      printf("会话角色无效，已退出登录。\n");
      return APP_ERR_INVALID_DATA;
    }

    int choice = -1;
    const CliInputStatus status = prompt_choice(input, "请选择：", &choice);
    if (status == CLI_INPUT_IO) {
      return APP_ERR_IO;
    }
    if (status == CLI_INPUT_EOF) {
      return APP_OK;
    }
    if (status == CLI_INPUT_TOO_LONG) {
      continue;
    }
    if (choice == 0) {
      printf("已退出登录。\n");
      return APP_OK;
    }
    if (choice < 0) {
      continue;
    }

    const AppResult result = dispatch_menu(input, actor, choice);
    if (input->io_error) {
      return APP_ERR_IO;
    }
    if (result_is_fatal(result)) {
      return result;
    }
    if (input->eof) {
      return APP_OK;
    }
  }
}

// 未登录菜单：登录后进入角色菜单，退出登录后回到本菜单。选择 0 或遇到 EOF
// 时结束。
static AppResult run_start_menu(CliInput *input) {
  for (;;) {
    if (input->io_error) {
      return APP_ERR_IO;
    }
    if (input->eof) {
      return APP_OK;
    }

    printf("\n==== 校园卡管理系统 ====\n");
    printf("1. 登录\n");
    printf("0. 退出\n");

    int choice = -1;
    const CliInputStatus status = prompt_choice(input, "请选择：", &choice);
    if (status == CLI_INPUT_IO) {
      return APP_ERR_IO;
    }
    if (status == CLI_INPUT_EOF) {
      return APP_OK;
    }
    if (status == CLI_INPUT_TOO_LONG) {
      continue;
    }
    if (choice == 0) {
      printf("程序退出。\n");
      return APP_OK;
    }
    if (choice != 1) {
      continue;
    }

    Account session = {0};
    bool logged_in = false;
    AppResult result = run_login(input, &session, &logged_in);
    if (result != APP_OK) {
      secure_clear(&session, sizeof session);
      return result;
    }
    if (!logged_in) {
      secure_clear(&session, sizeof session);
      if (input->eof) {
        return APP_OK;
      }
      continue;
    }

    result = run_role_menu(input, &session);
    secure_clear(&session, sizeof session);
    if (result != APP_OK) {
      return result;
    }
    if (input->eof) {
      return APP_OK;
    }
    // 退出登录后继续显示未登录菜单。
  }
}

AppResult cli_run() {
  // 程序入口由 main 管理存储生命周期；本函数只负责 CLI 交互和会话。
  configure_console_encoding();

  CliInput input = {.eof = false, .io_error = false};
  bool needs_bootstrap = false;
  AppResult result = auth_needs_bootstrap(&needs_bootstrap);
  if (result != APP_OK) {
    print_error(result);
    return result;
  }

  if (needs_bootstrap) {
    result = run_bootstrap(&input);
    if (result != APP_OK) {
      return result;
    }
  }

  result = run_start_menu(&input);
  if (result == APP_OK && (fflush(stdout) == EOF || ferror(stdout))) {
    return APP_ERR_IO;
  }
  return result;
}
