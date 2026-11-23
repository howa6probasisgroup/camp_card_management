#include "cli.h"

#include "services/account_service.h"
#include "services/auth_service.h"
#include "services/card_service.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#endif

/**
 * @file cli.c
 * @brief 实现 CLI 输入输出、三角色菜单和会话流程。
 * @details 权限与业务规则由服务层执行；金额使用整数分并检查溢出。提示只依据
 *          返回码，不输出路径、凭据或内部状态。
 */

/**
 * @brief 单行输入缓冲区容量。
 * @details 同时限制单次输入；超长行会排空，避免残留字符成为下一次选择。
 */
enum { CLI_LINE_CAPACITY = 4096 };

/**
 * @brief 单行输入的暂存状态。
 * @details 记录是否遇到 EOF、超长行或输入流故障，供调用方区分失败原因。
 */
typedef struct {
  char buffer[CLI_LINE_CAPACITY];
  size_t length;
  bool too_long;
  bool eof;
  bool io_error;
} CliLine;

/**
 * @brief 会话级输入状态。
 * @details 任一次读取遇到 EOF 或流故障后，菜单不再继续读取输入。
 */
typedef struct {
  bool eof;
  bool io_error;
} CliInput;

/**
 * @brief 单次输入读取结果。
 * @details EOF 表示正常结束；超长和输入流故障分别使用独立状态。
 */
typedef enum {
  CLI_INPUT_OK = 0,
  CLI_INPUT_EOF,
  CLI_INPUT_TOO_LONG,
  CLI_INPUT_IO
} CliInputStatus;

/**
 * @brief 判断字符是否为 ASCII 数字。
 * @details 显式比较，避免 locale 影响 `isdigit` 的结果。
 * @param[in] value 待判断的字符。
 * @return 字符位于 `0` 到 `9` 时返回 true，否则返回 false。
 */
static bool is_ascii_digit(const char value) {
  return value >= '0' && value <= '9';
}

/**
 * @brief 清除敏感内存。
 * @details 使用 `volatile` 写入，防止编译器把清零操作优化掉。
 * @param[in,out] data 待清除的缓冲区。
 * @param[in] size 缓冲区字节数。
 */
static void secure_clear(void *data, size_t size) {
  volatile unsigned char *cursor = data;
  while (size > 0) {
    *cursor++ = 0;
    size--;
  }
}

/**
 * @brief 终端回显控制结果。
 * @details 输入不是终端时不改变回显；终端回显切换失败时不得继续读取密码。
 */
typedef enum {
  CLI_ECHO_UNAVAILABLE = 0,
  CLI_ECHO_DISABLED,
  CLI_ECHO_FAILED
} CliEchoStatus;

/**
 * @brief 保存关闭回显前的终端状态。
 * @details 仅当 `active` 为 true 时保存的状态可用于恢复。
 */
typedef struct {
#ifdef _WIN32
  HANDLE handle;
  DWORD original_mode;
#else
  struct termios original_mode;
#endif
  bool active;
} CliEchoState;

/**
 * @brief 判断标准输入是否连接终端。
 * @details 只有终端输入需要关闭回显；重定向输入和测试桩不操作终端状态。
 * @return 标准输入连接终端时返回 true，否则返回 false。
 */
static bool cli_input_is_terminal() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

/**
 * @brief 关闭标准输入的终端回显。
 * @details 仅在标准输入连接终端时操作终端状态；重定向输入或测试桩不涉及
 *          回显控制。终端状态读取或写入失败时返回 `CLI_ECHO_FAILED`。
 * @param[out] state 接收恢复终端状态所需的数据。
 * @return 终端回显控制结果。
 */
static CliEchoStatus cli_disable_echo(CliEchoState *state) {
  if (state == nullptr) {
    return CLI_ECHO_FAILED;
  }
  memset(state, 0, sizeof *state);

#ifdef _WIN32
  if (!cli_input_is_terminal()) {
    return CLI_ECHO_UNAVAILABLE;
  }

  HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  if (input == nullptr || input == INVALID_HANDLE_VALUE ||
      !GetConsoleMode(input, &mode)) {
    return CLI_ECHO_FAILED;
  }
  if (!SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT)) {
    return CLI_ECHO_FAILED;
  }

  state->handle = input;
  state->original_mode = mode;
  state->active = true;
  return CLI_ECHO_DISABLED;
#else
  if (!cli_input_is_terminal()) {
    return CLI_ECHO_UNAVAILABLE;
  }

  struct termios mode;
  if (tcgetattr(STDIN_FILENO, &mode) != 0) {
    return CLI_ECHO_FAILED;
  }

  struct termios no_echo = mode;
  no_echo.c_lflag &= ~(tcflag_t)ECHO;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &no_echo) != 0) {
    return CLI_ECHO_FAILED;
  }

  state->original_mode = mode;
  state->active = true;
  return CLI_ECHO_DISABLED;
#endif
}

/**
 * @brief 恢复标准输入的终端回显状态。
 * @details 只恢复先前成功关闭过回显的终端；状态恢复失败时返回 false。
 * @param[in,out] state 恢复后清除活动标记的终端状态。
 * @return 成功恢复或无需恢复时返回 true，否则返回 false。
 */
static bool cli_restore_echo(CliEchoState *state) {
  if (state == nullptr || !state->active) {
    return true;
  }

  bool restored;
#ifdef _WIN32
  restored = SetConsoleMode(state->handle, state->original_mode) != 0;
#else
  restored = tcsetattr(STDIN_FILENO, TCSANOW, &state->original_mode) == 0;
#endif
  state->active = false;
  return restored;
}

#ifndef _WIN32
/**
 * @brief 密码输入期间需要恢复终端回显的信号数量。
 * @details 覆盖常见的可中断终止信号；忽略信号和自定义处理器的原有语义在
 *          读取结束后恢复。
 */
enum { CLI_ECHO_SIGNAL_COUNT = 4 };

/**
 * @brief 保存密码输入期间替换的信号处理器。
 * @details 只保存实际安装的信号，重放信号前先恢复全部原处理器。
 */
typedef struct {
  struct sigaction previous_actions[CLI_ECHO_SIGNAL_COUNT];
  int signals[CLI_ECHO_SIGNAL_COUNT];
  size_t count;
} CliSignalGuard;

static volatile sig_atomic_t cli_pending_signal = 0;

/**
 * @brief 记录中断密码读取的信号。
 * @details 处理器只写入 `sig_atomic_t`，终端状态由正常的读取清理路径恢复。
 * @param signal_number 收到的信号编号。
 */
static void cli_signal_handler(const int signal_number) {
  cli_pending_signal = signal_number;
}

/**
 * @brief 恢复密码输入前的信号处理器。
 * @param[in,out] guard 保存信号处理器的状态。
 * @return 全部恢复时返回 true，否则返回 false。
 */
static bool cli_restore_signal_guard(CliSignalGuard *guard) {
  if (guard == nullptr) {
    return false;
  }

  bool restored = true;
  for (size_t i = 0; i < guard->count; i++) {
    if (sigaction(guard->signals[i], &guard->previous_actions[i], nullptr) !=
        0) {
      restored = false;
    }
  }
  guard->count = 0;
  return restored;
}

/**
 * @brief 为密码输入临时安装不自动重启读取的信号处理器。
 * @details 原始处理器为 `SIG_IGN` 的信号保持不变。收到信号后，读取路径
 *          恢复回显与处理器，再通过 `raise` 重放原信号。
 * @param[out] guard 接收待恢复的处理器状态。
 * @return 安装成功时返回 true，否则返回 false。
 */
static bool cli_install_signal_guard(CliSignalGuard *guard) {
  if (guard == nullptr) {
    return false;
  }
  memset(guard, 0, sizeof *guard);
  cli_pending_signal = 0;

  struct sigaction action = {.sa_handler = cli_signal_handler};
  if (sigemptyset(&action.sa_mask) != 0) {
    return false;
  }
  action.sa_flags = 0;

  static constexpr int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT};
  for (size_t i = 0; i < sizeof signals / sizeof signals[0]; i++) {
    struct sigaction previous;
    if (sigaction(signals[i], nullptr, &previous) != 0) {
      (void)cli_restore_signal_guard(guard);
      return false;
    }
    if (previous.sa_handler == SIG_IGN) {
      continue;
    }
    if (sigaction(signals[i], &action,
                  &guard->previous_actions[guard->count]) != 0) {
      (void)cli_restore_signal_guard(guard);
      return false;
    }
    guard->signals[guard->count] = signals[i];
    guard->count++;
  }
  return true;
}
#endif

/**
 * @brief 排空当前行剩余字符。
 * @details 读取到换行或 EOF，避免超长输入残留内容被当作下一次选择，并记录
 *          排空过程中遇到的 EOF 或流故障。
 * @param[in,out] line 当前行的暂存状态。
 */
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

/**
 * @brief 读取一行输入。
 * @details 去掉行尾换行和 CRLF 中的回车，保留 UTF-8 原始字节；EOF 前已有
 *          内容时仍返回该行。
 * @param[out] line 接收行内容及读取状态。
 * @return 有可用行时返回 true；空输入遇到 EOF 或流故障时返回 false。
 */
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
    // 缓冲区已满且尚未到行尾，需隔离本行剩余内容。
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

/**
 * @brief 配置 Windows 控制台编码。
 * @details 将输入输出代码页固定为 UTF-8，避免与文件存储编码不一致；
 *          非 Windows 平台不执行操作。
 */
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

  // 至少一位 ASCII 整数，可带小数点后 1 到 2 位数字；不接受空白、正负号、
  // 指数或逗号。允许解析零，零金额业务由服务层拒绝。
  const char *cursor = text;
  if (*cursor == '\0' || !is_ascii_digit(*cursor)) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  // 先累积原始整数部分，无小数点时再按元换算为分。
  uint64_t cents = 0;
  while (is_ascii_digit(*cursor)) {
    const unsigned digit = (unsigned)(*cursor - '0');
    if (cents > (UINT64_MAX - digit) / 10) {
      return APP_ERR_INVALID_ARGUMENT;
    }
    cents = cents * 10 + digit;
    cursor++;
  }

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
      return APP_ERR_INVALID_ARGUMENT;
    }
    if (is_ascii_digit(*cursor)) {
      return APP_ERR_INVALID_ARGUMENT;
    }
    if (fractional_digits == 1) {
      fraction *= 10;
    }
    if (cents > (UINT64_MAX - fraction) / 100) {
      return APP_ERR_INVALID_ARGUMENT;
    }
    cents = cents * 100 + fraction;
  } else {
    if (cents > UINT64_MAX / 100) {
      return APP_ERR_INVALID_ARGUMENT;
    }
    cents *= 100;
  }

  if (*cursor != '\0') {
    return APP_ERR_INVALID_ARGUMENT;
  }

  // 只有全部校验通过才写出，失败时保持调用方原值。
  if (cents > (uint64_t)INT64_MAX) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  *out_cents = (int64_t)cents;
  return APP_OK;
}

/**
 * @brief 将整数分格式化为固定两位小数的元。
 * @details 不使用浮点数；参数无效、金额为负或缓冲区不足时失败。
 * @param[in] cents 待格式化的非负整数分金额。
 * @param[out] out 接收格式化文本的缓冲区。
 * @param[in] capacity 输出缓冲区容量。
 * @return 格式化结果完整写入时返回 true，否则返回 false。
 */
static bool format_amount(const int64_t cents, char *out,
                          const size_t capacity) {
  if (out == nullptr || capacity == 0 || cents < 0) {
    return false;
  }
  const int written = snprintf(out, capacity, "%" PRId64 ".%02d", cents / 100,
                               (int)(cents % 100));
  return written >= 0 && (size_t)written < capacity;
}

/**
 * @brief 输出用户可见的错误提示。
 * @details 提示只依据返回码生成，不附加路径、凭据或内部状态。
 * @param[in] result 服务或存储接口返回的结果码。
 */
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

/**
 * @brief 判断错误是否终止当前流程。
 * @details I/O 故障、数据损坏和内存不足不能继续；业务校验错误仍可留在菜单重试。
 * @param[in] result 待判断的结果码。
 * @return 应终止流程时返回 true，否则返回 false。
 */
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

/**
 * @brief 输出账号摘要。
 * @details 只显示用户名、角色和关联学号，不输出盐、派生值或其他凭据。
 * @param[in] account 待显示的账号记录。
 */
static void print_account(const Account *account) {
  printf("用户名：%s\n", account->username);
  printf("角色：%s\n", role_text(account->role));
  if (account->role == ROLE_CARD_USER) {
    printf("关联学号：%s\n", account->student_id);
  } else {
    printf("关联学号：无\n");
  }
}

/**
 * @brief 输出校园卡记录。
 * @details 余额由整数分格式化，不使用浮点数。
 * @param[in] record 待显示的校园卡记录。
 */
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

/**
 * @brief 提示并读取一行输入。
 * @details 超长输入已在该函数内排空并提示；EOF 与流故障记录到会话状态。
 *          密码等敏感输入在终端中关闭回显，并在读取结束后恢复终端状态。
 * @param[in,out] input 会话级输入状态。
 * @param[in] label 输出给用户的提示文本。
 * @param[out] out 接收读取内容的缓冲区。
 * @param[in] capacity 输出缓冲区容量。
 * @param[in] hide_echo 是否在终端输入期间关闭回显。
 * @return 读取结果状态。
 */
static CliInputStatus prompt_line_internal(CliInput *input, const char *label,
                                           char *out, const size_t capacity,
                                           const bool hide_echo) {
  printf("%s", label);
  if (fflush(stdout) != 0) {
    input->io_error = true;
    return CLI_INPUT_IO;
  }

  CliEchoState echo_state = {0};
  CliEchoStatus echo_status = CLI_ECHO_UNAVAILABLE;
#ifndef _WIN32
  CliSignalGuard signal_guard = {.count = 0};
  bool signal_guard_active = false;
  if (hide_echo && cli_input_is_terminal()) {
    if (!cli_install_signal_guard(&signal_guard)) {
      input->io_error = true;
      return CLI_INPUT_IO;
    }
    signal_guard_active = true;
  }
#endif

  if (hide_echo) {
    echo_status = cli_disable_echo(&echo_state);
    if (echo_status == CLI_ECHO_FAILED) {
#ifndef _WIN32
      if (signal_guard_active) {
        (void)cli_restore_signal_guard(&signal_guard);
      }
#endif
      input->io_error = true;
      return CLI_INPUT_IO;
    }
  }

  CliLine line = {0};
  bool has_line = false;
  bool echo_restored = true;
  bool signal_restored = true;
  bool newline_written = true;
#ifndef _WIN32
  sig_atomic_t interrupted_signal = cli_pending_signal;
  cli_pending_signal = 0;
  if (interrupted_signal == 0) {
    has_line = cli_read_line(&line);
  }
#else
  has_line = cli_read_line(&line);
#endif

  if (echo_status == CLI_ECHO_DISABLED) {
    echo_restored = cli_restore_echo(&echo_state);
    newline_written = printf("\n") != EOF;
  }

#ifndef _WIN32
  if (signal_guard_active) {
    signal_restored = cli_restore_signal_guard(&signal_guard);
    if (interrupted_signal == 0) {
      interrupted_signal = cli_pending_signal;
    }
    cli_pending_signal = 0;
  }
  if (interrupted_signal != 0) {
    if (signal_restored) {
      (void)raise(interrupted_signal);
    }
    input->io_error = true;
    secure_clear(line.buffer, sizeof line.buffer);
    return CLI_INPUT_IO;
  }
#endif

  if (!signal_restored || !echo_restored || !newline_written) {
    input->io_error = true;
    secure_clear(line.buffer, sizeof line.buffer);
    return CLI_INPUT_IO;
  }

  if (!has_line) {
    secure_clear(line.buffer, sizeof line.buffer);
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

/**
 * @brief 提示并读取普通一行输入。
 * @details 使用正常终端回显，适用于用户名和业务字段。
 * @param[in,out] input 会话级输入状态。
 * @param[in] label 输出给用户的提示文本。
 * @param[out] out 接收读取内容的缓冲区。
 * @param[in] capacity 输出缓冲区容量。
 * @return 读取结果状态。
 */
static CliInputStatus prompt_line(CliInput *input, const char *label, char *out,
                                  const size_t capacity) {
  return prompt_line_internal(input, label, out, capacity, false);
}

/**
 * @brief 提示并读取密码输入。
 * @details 在终端中关闭回显，读取完成或失败后恢复终端状态。
 * @param[in,out] input 会话级输入状态。
 * @param[in] label 输出给用户的提示文本。
 * @param[out] out 接收读取内容的缓冲区。
 * @param[in] capacity 输出缓冲区容量。
 * @return 读取结果状态。
 */
static CliInputStatus prompt_secret_line(CliInput *input, const char *label,
                                         char *out, const size_t capacity) {
  return prompt_line_internal(input, label, out, capacity, true);
}

/**
 * @brief 读取单数字菜单编号。
 * @details 空行、多个字符和非数字都视为无效选择；无效选择返回
 *          `CLI_INPUT_OK`，由调用方重新显示菜单。
 * @param[in,out] input 会话级输入状态。
 * @param[in] label 输出给用户的提示文本。
 * @param[out] choice 接收菜单编号；无效选择时写入 `-1`。
 * @return 读取结果状态。
 */
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

/**
 * @brief 执行首次管理员初始化。
 * @details 输入不合法时留在初始化流程重试；账号已存在时提示直接登录。EOF
 *          或流故障由调用方根据输入状态处理。
 * @param[in,out] input 会话级输入状态。
 * @return 初始化成功、可继续登录或正常结束时返回 `APP_OK`；致命错误原样返回。
 */
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
      status = prompt_secret_line(input, "密码：", password, sizeof password);
    }
    if (status == CLI_INPUT_OK) {
      status = prompt_secret_line(input, "确认密码：", confirm, sizeof confirm);
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

/**
 * @brief 执行登录。
 * @details 认证失败只提示并回到未登录菜单；存储损坏等致命错误向上传播。
 * @param[in,out] input 会话级输入状态。
 * @param[out] session 成功时接收登录会话。
 * @param[out] logged_in 接收是否登录成功。
 * @return 未发生致命错误时返回 `APP_OK`，否则返回对应错误码。
 */
static AppResult run_login(CliInput *input, Account *session, bool *logged_in) {
  *logged_in = false;
  char username[USERNAME_MAX_BYTES + 1] = {0};
  char password[PASSWORD_MAX_BYTES + 1] = {0};

  CliInputStatus status =
      prompt_line(input, "用户名：", username, sizeof username);
  if (status != CLI_INPUT_OK) {
    return status == CLI_INPUT_IO ? APP_ERR_IO : APP_OK;
  }
  status = prompt_secret_line(input, "密码：", password, sizeof password);
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

/**
 * @brief 读取处理函数所需的字段。
 * @details 输入失败时通过 `goto cleanup` 结束当前处理函数；返回 `APP_OK`
 *          表示已处理完（成功或可重试的业务错误）。
 * @param input 会话级输入状态。
 * @param label 输出给用户的提示文本。
 * @param buffer 接收输入内容的缓冲区。
 */
#define ASK_OR_FINISH(input, label, buffer)                                    \
  do {                                                                         \
    const CliInputStatus ask_status_ =                                         \
        prompt_line((input), (label), (buffer), sizeof(buffer));               \
    if (ask_status_ != CLI_INPUT_OK) {                                         \
      result = ask_status_ == CLI_INPUT_IO ? APP_ERR_IO : APP_OK;              \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0)

#define ASK_SECRET_OR_FINISH(input, label, buffer)                             \
  do {                                                                         \
    const CliInputStatus ask_status_ =                                         \
        prompt_secret_line((input), (label), (buffer), sizeof(buffer));        \
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
  ASK_SECRET_OR_FINISH(input, "新账号密码：", password);
  ASK_SECRET_OR_FINISH(input, "确认密码：", confirm);
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

  ASK_SECRET_OR_FINISH(input, "旧密码：", old_password);
  ASK_SECRET_OR_FINISH(input, "新密码：", new_password);
  ASK_SECRET_OR_FINISH(input, "确认新密码：", confirm);
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

/**
 * @brief 处理充值或消费。
 * @details 金额先由 CLI 解析为整数分，零金额仍交给服务层拒绝。
 * @param[in,out] input 会话级输入状态。
 * @param[in] actor 当前登录的卡用户会话。
 * @param[in] recharge 为 true 时充值，为 false 时消费。
 * @return 未发生致命错误时返回 `APP_OK`，否则返回对应错误码。
 */
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

/**
 * @brief 按角色分发菜单编号。
 * @details 返回业务结果供角色菜单判断是否为致命错误；用户提示已由处理函数
 *          输出。
 * @param[in,out] input 会话级输入状态。
 * @param[in] actor 当前登录的调用者会话。
 * @param[in] choice 菜单编号。
 * @return 处理结果；无效菜单编号返回 `APP_OK`。
 */
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

/**
 * @brief 运行登录后的角色菜单。
 * @details 退出登录返回 `APP_OK`，由外层回到未登录菜单；致命错误向上传播。
 *          EOF 由调用方根据输入状态结束程序。
 * @param[in,out] input 会话级输入状态。
 * @param[in] actor 当前登录的调用者会话。
 * @return 正常退出或 EOF 时返回 `APP_OK`，否则返回对应错误码。
 */
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

/**
 * @brief 运行未登录菜单。
 * @details 登录后进入角色菜单，退出登录后回到本菜单；选择 `0` 或遇到 EOF
 * 时结束。
 * @param[in,out] input 会话级输入状态。
 * @return 正常退出或 EOF 时返回 `APP_OK`，否则返回对应错误码。
 */
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
  }
}

AppResult cli_run() {
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
