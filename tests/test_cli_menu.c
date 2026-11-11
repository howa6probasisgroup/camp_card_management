// CLI 菜单与业务场景测试。通过包含 src/cli.c 替换输入函数，并用测试桩
// 代替业务服务，验证三角色菜单的调用映射、会话流程、输入边界和展示内容。
// 真实权限、余额与持久化规则由服务层测试覆盖，本测试不复制这些规则。
//
// 输出重定向到临时文件，用于检查账号摘要不包含盐或派生值、密码不会被界面
// 主动打印。检查结果写到 stderr，避免与待检查的标准输出混在一起。

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cli_test_getchar();
static int cli_test_ferror(FILE *stream);

#define getchar cli_test_getchar
#define ferror cli_test_ferror

#include "../src/cli.c"

#undef getchar
#undef ferror

static const char *g_input;
static size_t g_position;
static bool g_force_error;
static bool g_fail_output_on_read;

static int cli_test_getchar() {
  if (g_fail_output_on_read) {
    g_fail_output_on_read = false;
    // 最后一次提示已刷新，此后的输出写入会失败。
    if (freopen("cli_menu_test_output.txt", "r", stdout) == nullptr) {
      fprintf(stderr, "test setup error: cannot make stdout read-only\n");
      exit(EXIT_FAILURE);
    }
  }
  if (g_input == nullptr || g_input[g_position] == '\0') {
    return EOF;
  }
  return (unsigned char)g_input[g_position++];
}

static int cli_test_ferror(FILE *stream) {
  return stream == stdin ? g_force_error : ferror(stream);
}

static size_t g_checks;
static size_t g_failures;

static void check(const bool condition, const char *expression,
                  const int line) {
  g_checks++;
  if (!condition) {
    g_failures++;
    fprintf(stderr, "FAIL line %d: %s\n", line, expression);
  }
}

#define CHECK(condition) check((condition), #condition, __LINE__)

static void copy_text(char *destination, const size_t capacity,
                      const char *source) {
  if (capacity == 0) {
    return;
  }
  (void)snprintf(destination, capacity, "%s", source);
}

static void make_record(StudentCardRecord *record, const char *student_id,
                        const char *name, const int64_t balance_cents,
                        const CardStatus status) {
  memset(record, 0, sizeof *record);
  copy_text(record->student_id, sizeof record->student_id, student_id);
  copy_text(record->name, sizeof record->name, name);
  copy_text(record->sex, sizeof record->sex, "男");
  copy_text(record->department, sizeof record->department, "计算机学院");
  copy_text(record->class_name, sizeof record->class_name, "24154B1");
  copy_text(record->phone, sizeof record->phone, "13800000000");
  record->balance_cents = balance_cents;
  record->status = status;
}

static bool g_needs_bootstrap;
static AppResult g_needs_result;
static AppResult g_bootstrap_result;
static AppResult g_login_result;
static AppResult g_change_result;
static AppResult g_account_create_result;
static AppResult g_account_get_result;
static AppResult g_account_list_result;
static AppResult g_account_delete_result;
static AppResult g_card_issue_result;
static AppResult g_card_get_result;
static AppResult g_card_find_result;
static AppResult g_phone_result;
static AppResult g_recharge_result;
static AppResult g_consume_result;
static AppResult g_status_result;

static Account g_login_account;
static StudentCardRecord g_card;
static StudentCardRecord g_find_records[2];
static size_t g_find_count;
static int64_t g_recharge_balance;
static int64_t g_consume_balance;

static size_t g_bootstrap_calls;
static size_t g_login_calls;
static size_t g_create_calls;
static size_t g_get_calls;
static size_t g_list_calls;
static size_t g_delete_calls;
static size_t g_issue_calls;
static size_t g_card_get_calls;
static size_t g_find_calls;
static size_t g_phone_calls;
static size_t g_recharge_calls;
static size_t g_consume_calls;
static size_t g_status_calls;
static size_t g_change_calls;

static char g_bootstrap_username[USERNAME_MAX_BYTES + 1];
static char g_bootstrap_password[PASSWORD_MAX_BYTES + 1];
static char g_created_username[USERNAME_MAX_BYTES + 1];
static char g_created_password[PASSWORD_MAX_BYTES + 1];
static char g_created_student_id[STUDENT_ID_LENGTH + 1];
static UserRole g_created_role;
static char g_deleted_username[USERNAME_MAX_BYTES + 1];
static StudentProfileInput g_issued_profile;
static char g_requested_student_id[STUDENT_ID_LENGTH + 1];
static char g_updated_phone[PHONE_MAX_BYTES + 1];
static int64_t g_recharge_amount;
static int64_t g_consume_amount;
static char g_status_student_id[STUDENT_ID_LENGTH + 1];
static CardStatus g_status_value;
static char g_change_old[PASSWORD_MAX_BYTES + 1];
static char g_change_new[PASSWORD_MAX_BYTES + 1];

static void reset_fakes() {
  g_needs_bootstrap = false;
  g_needs_result = APP_OK;
  g_bootstrap_result = APP_OK;
  g_login_result = APP_OK;
  g_change_result = APP_OK;
  g_account_create_result = APP_OK;
  g_account_get_result = APP_OK;
  g_account_list_result = APP_OK;
  g_account_delete_result = APP_OK;
  g_card_issue_result = APP_OK;
  g_card_get_result = APP_OK;
  g_card_find_result = APP_OK;
  g_phone_result = APP_OK;
  g_recharge_result = APP_OK;
  g_consume_result = APP_OK;
  g_status_result = APP_OK;

  memset(&g_login_account, 0, sizeof g_login_account);
  copy_text(g_login_account.username, sizeof g_login_account.username, "admin");
  g_login_account.role = ROLE_ADMIN;

  make_record(&g_card, "251543201", "张三", 1234, CARD_STATUS_ACTIVE);
  make_record(&g_find_records[0], "251543201", "张三", 0, CARD_STATUS_ACTIVE);
  make_record(&g_find_records[1], "251543202", "张三", 500, CARD_STATUS_FROZEN);
  g_find_count = 0;
  g_recharge_balance = 0;
  g_consume_balance = 0;

  g_bootstrap_calls = 0;
  g_login_calls = 0;
  g_create_calls = 0;
  g_get_calls = 0;
  g_list_calls = 0;
  g_delete_calls = 0;
  g_issue_calls = 0;
  g_card_get_calls = 0;
  g_find_calls = 0;
  g_phone_calls = 0;
  g_recharge_calls = 0;
  g_consume_calls = 0;
  g_status_calls = 0;
  g_change_calls = 0;

  memset(g_bootstrap_username, 0, sizeof g_bootstrap_username);
  memset(g_bootstrap_password, 0, sizeof g_bootstrap_password);
  memset(g_created_username, 0, sizeof g_created_username);
  memset(g_created_password, 0, sizeof g_created_password);
  memset(g_created_student_id, 0, sizeof g_created_student_id);
  g_created_role = ROLE_ADMIN;
  memset(g_deleted_username, 0, sizeof g_deleted_username);
  memset(&g_issued_profile, 0, sizeof g_issued_profile);
  memset(g_requested_student_id, 0, sizeof g_requested_student_id);
  memset(g_updated_phone, 0, sizeof g_updated_phone);
  g_recharge_amount = 0;
  g_consume_amount = 0;
  memset(g_status_student_id, 0, sizeof g_status_student_id);
  g_status_value = CARD_STATUS_ACTIVE;
  memset(g_change_old, 0, sizeof g_change_old);
  memset(g_change_new, 0, sizeof g_change_new);

  g_input = nullptr;
  g_position = 0;
  g_force_error = false;
  g_fail_output_on_read = false;
}

AppResult auth_needs_bootstrap(bool *out_needed) {
  if (out_needed == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (g_needs_result != APP_OK) {
    return g_needs_result;
  }
  *out_needed = g_needs_bootstrap;
  return APP_OK;
}

AppResult auth_bootstrap_admin(const char *username, const char *password) {
  g_bootstrap_calls++;
  copy_text(g_bootstrap_username, sizeof g_bootstrap_username, username);
  copy_text(g_bootstrap_password, sizeof g_bootstrap_password, password);
  return g_bootstrap_result;
}

AppResult auth_login(const char *username, const char *password, Account *out) {
  (void)username;
  (void)password;
  g_login_calls++;
  if (g_login_result != APP_OK) {
    return g_login_result;
  }
  *out = g_login_account;
  return APP_OK;
}

AppResult auth_change_password(const Account *actor, const char *old_password,
                               const char *new_password) {
  (void)actor;
  g_change_calls++;
  copy_text(g_change_old, sizeof g_change_old, old_password);
  copy_text(g_change_new, sizeof g_change_new, new_password);
  return g_change_result;
}

AppResult account_create(const Account *actor, const char *username,
                         const char *password, const UserRole role,
                         const char *student_id) {
  (void)actor;
  g_create_calls++;
  copy_text(g_created_username, sizeof g_created_username, username);
  copy_text(g_created_password, sizeof g_created_password, password);
  g_created_role = role;
  copy_text(g_created_student_id, sizeof g_created_student_id, student_id);
  return g_account_create_result;
}

AppResult account_get(const Account *actor, const char *username,
                      Account *out) {
  (void)actor;
  g_get_calls++;
  if (g_account_get_result != APP_OK) {
    return g_account_get_result;
  }
  memset(out, 0, sizeof *out);
  copy_text(out->username, sizeof out->username, username);
  out->role = ROLE_CARD_USER;
  copy_text(out->student_id, sizeof out->student_id, "251543201");
  copy_text(out->password_salt_hex, sizeof out->password_salt_hex,
            "sentinel-salt-value");
  copy_text(out->password_hash_hex, sizeof out->password_hash_hex,
            "sentinel-hash-value"
            "00112233445566778899aabbccddeeff");
  return APP_OK;
}

AppResult account_list(const Account *actor, AccountList *out) {
  (void)actor;
  g_list_calls++;
  if (g_account_list_result != APP_OK) {
    return g_account_list_result;
  }

  Account account = {0};
  copy_text(account.username, sizeof account.username, "admin");
  account.role = ROLE_ADMIN;
  if (!account_list_append(out, &account)) {
    return APP_ERR_NO_MEMORY;
  }

  account = (Account){0};
  copy_text(account.username, sizeof account.username, "target");
  copy_text(account.password_salt_hex, sizeof account.password_salt_hex,
            "sentinel-salt-value");
  copy_text(account.password_hash_hex, sizeof account.password_hash_hex,
            "sentinel-hash-value"
            "00112233445566778899aabbccddeeff");
  account.role = ROLE_CARD_USER;
  copy_text(account.student_id, sizeof account.student_id, "251543201");
  if (!account_list_append(out, &account)) {
    return APP_ERR_NO_MEMORY;
  }
  return APP_OK;
}

AppResult account_delete(const Account *actor, const char *username) {
  (void)actor;
  g_delete_calls++;
  copy_text(g_deleted_username, sizeof g_deleted_username, username);
  return g_account_delete_result;
}

AppResult card_issue(const Account *actor, const StudentProfileInput *input) {
  (void)actor;
  g_issue_calls++;
  g_issued_profile = *input;
  return g_card_issue_result;
}

AppResult card_get_by_id(const Account *actor, const char *student_id,
                         StudentCardRecord *out) {
  g_card_get_calls++;
  copy_text(g_requested_student_id, sizeof g_requested_student_id, student_id);
  if (g_card_get_result != APP_OK) {
    return g_card_get_result;
  }
  (void)actor;
  *out = g_card;
  return APP_OK;
}

AppResult card_find_by_name(const Account *actor, const char *name,
                            StudentList *out) {
  (void)actor;
  (void)name;
  g_find_calls++;
  if (g_card_find_result != APP_OK) {
    return g_card_find_result;
  }
  for (size_t i = 0; i < g_find_count; i++) {
    if (!student_list_append(out, &g_find_records[i])) {
      return APP_ERR_NO_MEMORY;
    }
  }
  return APP_OK;
}

AppResult card_update_own_phone(const Account *actor, const char *phone) {
  (void)actor;
  g_phone_calls++;
  copy_text(g_updated_phone, sizeof g_updated_phone, phone);
  return g_phone_result;
}

AppResult card_recharge(const Account *actor, const int64_t amount_cents,
                        int64_t *new_balance_cents) {
  (void)actor;
  g_recharge_calls++;
  g_recharge_amount = amount_cents;
  if (g_recharge_result != APP_OK) {
    return g_recharge_result;
  }
  *new_balance_cents = g_recharge_balance;
  return APP_OK;
}

AppResult card_consume(const Account *actor, const int64_t amount_cents,
                       int64_t *new_balance_cents) {
  (void)actor;
  g_consume_calls++;
  g_consume_amount = amount_cents;
  if (g_consume_result != APP_OK) {
    return g_consume_result;
  }
  *new_balance_cents = g_consume_balance;
  return APP_OK;
}

AppResult card_set_status(const Account *actor, const char *student_id,
                          const CardStatus status) {
  (void)actor;
  g_status_calls++;
  copy_text(g_status_student_id, sizeof g_status_student_id, student_id);
  g_status_value = status;
  return g_status_result;
}

static AppResult run_input(const char *text) {
  g_input = text;
  g_position = 0;
  g_force_error = false;
  return cli_run();
}

static void begin_capture() {
  if (freopen("cli_menu_test_output.txt", "w", stdout) == nullptr) {
    fprintf(stderr, "test setup error: cannot redirect stdout\n");
    exit(EXIT_FAILURE);
  }
  if (setvbuf(stdout, nullptr, _IOFBF, BUFSIZ) != 0) {
    fprintf(stderr, "test setup error: cannot buffer stdout\n");
    exit(EXIT_FAILURE);
  }
}

static char *load_capture() {
  FILE *stream = fopen("cli_menu_test_output.txt", "rb");
  if (stream == nullptr) {
    return nullptr;
  }
  if (fseek(stream, 0, SEEK_END) != 0) {
    fclose(stream);
    return nullptr;
  }
  const long size = ftell(stream);
  if (size < 0 || fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return nullptr;
  }
  char *data = malloc((size_t)size + 1);
  if (data == nullptr) {
    fclose(stream);
    return nullptr;
  }
  const size_t read = fread(data, 1, (size_t)size, stream);
  fclose(stream);
  data[read] = '\0';
  return data;
}

static bool contains(const char *text, const char *needle) {
  return text != nullptr && strstr(text, needle) != nullptr;
}

static void test_bootstrap_and_admin_login() {
  reset_fakes();
  g_needs_bootstrap = true;
  const char *script = "admin\n"
                       "Admin1234\n"
                       "Different1\n"
                       "admin\n"
                       "Admin1234\n"
                       "Admin1234\n"
                       "1\n"
                       "admin\n"
                       "Admin1234\n"
                       "1\n"
                       "clerk01\n"
                       "Clerk1234\n"
                       "Clerk1234\n"
                       "3\n"
                       "0\n"
                       "0\n";

  CHECK(run_input(script) == APP_OK);
  CHECK(g_bootstrap_calls == 1);
  CHECK(strcmp(g_bootstrap_username, "admin") == 0);
  CHECK(strcmp(g_bootstrap_password, "Admin1234") == 0);
  CHECK(g_login_calls == 1);
  CHECK(g_create_calls == 1);
  CHECK(strcmp(g_created_username, "clerk01") == 0);
  CHECK(g_created_role == ROLE_CARD_CLERK);
  CHECK(g_created_student_id[0] == '\0');
}

static void test_bootstrap_retries_overlong_fields() {
  const struct {
    const char *prefix;
    size_t max_bytes;
  } cases[] = {
      {.prefix = "", .max_bytes = USERNAME_MAX_BYTES},
      {.prefix = "discarded\n", .max_bytes = PASSWORD_MAX_BYTES},
      {.prefix = "discarded\nFirstPass1\n", .max_bytes = PASSWORD_MAX_BYTES},
  };
  constexpr char retry[] = "\nadmin\nAdmin1234\nAdmin1234\n0\n";
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    reset_fakes();
    g_needs_bootstrap = true;
    char script[sizeof "discarded\nFirstPass1\n" + USERNAME_MAX_BYTES +
                PASSWORD_MAX_BYTES + sizeof retry];
    const size_t prefix_length = strlen(cases[i].prefix);
    memcpy(script, cases[i].prefix, prefix_length);
    memset(script + prefix_length, 'x', cases[i].max_bytes + 1);
    memcpy(script + prefix_length + cases[i].max_bytes + 1, retry,
           sizeof retry);

    CHECK(run_input(script) == APP_OK);
    CHECK(g_bootstrap_calls == 1);
    CHECK(strcmp(g_bootstrap_username, "admin") == 0);
    CHECK(strcmp(g_bootstrap_password, "Admin1234") == 0);
  }
}

static void test_admin_menu() {
  reset_fakes();
  const char *script = "1\n"
                       "admin\n"
                       "Admin1234\n"
                       "2\n"
                       "target\n"
                       "3\n"
                       "4\n"
                       "old\n"
                       "5\n"
                       "Admin1234\n"
                       "NewPass123\n"
                       "NewPass123\n"
                       "0\n"
                       "0\n";

  CHECK(run_input(script) == APP_OK);
  CHECK(g_get_calls == 1);
  CHECK(g_list_calls == 1);
  CHECK(g_delete_calls == 1);
  CHECK(strcmp(g_deleted_username, "old") == 0);
  CHECK(g_change_calls == 1);
  CHECK(strcmp(g_change_old, "Admin1234") == 0);
  CHECK(strcmp(g_change_new, "NewPass123") == 0);
}

static void test_card_user_menu() {
  reset_fakes();
  g_login_account.role = ROLE_CARD_USER;
  copy_text(g_login_account.username, sizeof g_login_account.username,
            "user01");
  copy_text(g_login_account.student_id, sizeof g_login_account.student_id,
            "251543201");
  g_recharge_balance = 1344;
  g_consume_balance = 1310;

  const char *script = "1\n"
                       "user01\n"
                       "User1234\n"
                       "1\n"
                       "2\n"
                       "13800000000\n"
                       "3\n"
                       "1.10\n"
                       "4\n"
                       "0.34\n"
                       "4\n"
                       "1.234\n"
                       "5\n"
                       "User1234\n"
                       "NewPass123\n"
                       "NewPass123\n"
                       "0\n"
                       "0\n";

  CHECK(run_input(script) == APP_OK);
  CHECK(g_card_get_calls == 1);
  CHECK(strcmp(g_requested_student_id, "251543201") == 0);
  CHECK(g_phone_calls == 1);
  CHECK(strcmp(g_updated_phone, "13800000000") == 0);
  CHECK(g_recharge_calls == 1);
  CHECK(g_recharge_amount == 110);
  CHECK(g_consume_calls == 1);
  CHECK(g_consume_amount == 34);
  CHECK(g_change_calls == 1);
}

static void test_clerk_menu() {
  reset_fakes();
  g_login_account.role = ROLE_CARD_CLERK;
  copy_text(g_login_account.username, sizeof g_login_account.username,
            "clerk01");
  g_find_count = 2;

  const char *script = "1\n"
                       "clerk01\n"
                       "Clerk1234\n"
                       "1\n"
                       "251543201\n"
                       "张三\n"
                       "男\n"
                       "计算机学院\n"
                       "24154B1\n"
                       "13800000000\n"
                       "2\n"
                       "251543201\n"
                       "3\n"
                       "张三\n"
                       "4\n"
                       "251543201\n"
                       "5\n"
                       "251543201\n"
                       "0\n"
                       "0\n";

  CHECK(run_input(script) == APP_OK);
  CHECK(g_issue_calls == 1);
  CHECK(strcmp(g_issued_profile.student_id, "251543201") == 0);
  CHECK(strcmp(g_issued_profile.name, "张三") == 0);
  CHECK(strcmp(g_issued_profile.sex, "男") == 0);
  CHECK(strcmp(g_issued_profile.department, "计算机学院") == 0);
  CHECK(strcmp(g_issued_profile.class_name, "24154B1") == 0);
  CHECK(strcmp(g_issued_profile.phone, "13800000000") == 0);
  CHECK(g_card_get_calls == 1);
  CHECK(g_find_calls == 1);
  CHECK(g_status_calls == 2);
  CHECK(strcmp(g_status_student_id, "251543201") == 0);
  CHECK(g_status_value == CARD_STATUS_ACTIVE);
}

static void test_admin_create_card_user() {
  reset_fakes();
  const char *script = "1\n"
                       "admin\n"
                       "Admin1234\n"
                       "1\n"
                       "card_user\n"
                       "CardPass1\n"
                       "CardPass1\n"
                       "2\n"
                       "251543201\n"
                       "0\n"
                       "0\n";

  CHECK(run_input(script) == APP_OK);
  CHECK(g_create_calls == 1);
  CHECK(g_created_role == ROLE_CARD_USER);
  CHECK(strcmp(g_created_student_id, "251543201") == 0);
}

static void test_login_failure_and_input_errors() {
  reset_fakes();
  g_login_result = APP_ERR_AUTH_FAILED;

  const char *prefix = "1\nnobody\nBadPass123\n9\n\n";
  constexpr size_t long_length = 5000;
  const size_t capacity = strlen(prefix) + long_length + 8;
  char *script = malloc(capacity);
  CHECK(script != nullptr);
  if (script == nullptr) {
    return;
  }

  size_t position = 0;
  memcpy(script + position, prefix, strlen(prefix));
  position += strlen(prefix);
  memset(script + position, 'x', long_length);
  position += long_length;
  script[position++] = '\n';
  script[position++] = '0';
  script[position++] = '\n';
  script[position] = '\0';

  CHECK(run_input(script) == APP_OK);
  CHECK(g_login_calls == 1);
  free(script);
}

static void test_bootstrap_storage_error() {
  reset_fakes();
  g_needs_result = APP_ERR_INVALID_DATA;

  CHECK(run_input("") == APP_ERR_INVALID_DATA);
  CHECK(g_bootstrap_calls == 0);
}

static void test_immediate_eof() {
  reset_fakes();
  CHECK(run_input("") == APP_OK);
}

static void test_final_argument_at_eof() {
  const struct {
    AppResult service_result;
    AppResult cli_result;
  } cases[] = {
      {.service_result = APP_ERR_IO, .cli_result = APP_ERR_IO},
      {.service_result = APP_ERR_INVALID_DATA,
       .cli_result = APP_ERR_INVALID_DATA},
      {.service_result = APP_ERR_NO_MEMORY, .cli_result = APP_ERR_NO_MEMORY},
      {.service_result = APP_ERR_NOT_FOUND, .cli_result = APP_OK},
      {.service_result = APP_OK, .cli_result = APP_OK},
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    reset_fakes();
    g_account_delete_result = cases[i].service_result;
    CHECK(run_input("1\nadmin\nAdmin1234\n4\nold") == cases[i].cli_result);
    CHECK(g_delete_calls == 1);
    CHECK(strcmp(g_deleted_username, "old") == 0);
  }
}

static void test_final_output() {
  reset_fakes();
  begin_capture();
  CHECK(run_input("0\n") == APP_OK);
  char *output = load_capture();
  CHECK(contains(output, "程序退出。"));
  free(output);

  // 显式退出和 EOF 都可能在最后一次提示之后输出消息。
  const char *scripts[] = {"0\n", "x"};
  for (size_t i = 0; i < sizeof scripts / sizeof scripts[0]; i++) {
    reset_fakes();
    begin_capture();
    g_fail_output_on_read = true;
    CHECK(run_input(scripts[i]) == APP_ERR_IO);
    CHECK(ferror(stdout) != 0);
  }
}

static void test_format_amount() {
  char buffer[32];
  CHECK(format_amount(0, buffer, sizeof buffer));
  CHECK(strcmp(buffer, "0.00") == 0);
  CHECK(format_amount(1, buffer, sizeof buffer));
  CHECK(strcmp(buffer, "0.01") == 0);
  CHECK(format_amount(1234, buffer, sizeof buffer));
  CHECK(strcmp(buffer, "12.34") == 0);
  CHECK(format_amount(INT64_MAX, buffer, sizeof buffer));
  CHECK(strcmp(buffer, "92233720368547758.07") == 0);

  char tiny[4];
  CHECK(!format_amount(1234, tiny, sizeof tiny));
  CHECK(!format_amount(-1, buffer, sizeof buffer));
}

static void test_rendered_output(const char *output) {
  CHECK(contains(output, "首次启动：创建管理员"));
  CHECK(contains(output, "管理员菜单"));
  CHECK(contains(output, "卡用户菜单"));
  CHECK(contains(output, "卡业务员菜单"));
  CHECK(contains(output, "共 2 个账号"));
  CHECK(contains(output, "用户名：target"));
  CHECK(contains(output, "关联学号：251543201"));
  CHECK(contains(output, "共找到 2 条同名记录"));
  CHECK(contains(output, "251543202"));
  CHECK(contains(output, "13.44"));
  CHECK(contains(output, "13.10"));
  CHECK(contains(output, "输入过长，已忽略该行"));
  CHECK(contains(output, "用户名或密码错误"));

  // 账号摘要不得输出盐、派生值和密码本身。
  CHECK(!contains(output, "sentinel-salt"));
  CHECK(!contains(output, "sentinel-hash"));
  CHECK(!contains(output, "password_salt"));
  CHECK(!contains(output, "password_hash"));
  CHECK(!contains(output, "Admin1234"));
  CHECK(!contains(output, "User1234"));
  CHECK(!contains(output, "Clerk1234"));
  CHECK(!contains(output, "NewPass123"));
}

int main() {
  begin_capture();

  test_format_amount();
  test_bootstrap_and_admin_login();
  test_bootstrap_retries_overlong_fields();
  test_admin_menu();
  test_card_user_menu();
  test_clerk_menu();
  test_admin_create_card_user();
  test_login_failure_and_input_errors();
  test_bootstrap_storage_error();
  test_final_argument_at_eof();
  test_immediate_eof();

  char *output = load_capture();
  CHECK(output != nullptr);
  test_rendered_output(output);
  test_final_output();

  if (g_failures == 0) {
    fprintf(stderr, "test_cli_menu: %zu checks passed\n", g_checks);
  } else {
    fprintf(stderr, "test_cli_menu: %zu of %zu checks failed\n", g_failures,
            g_checks);
  }

  free(output);
  fclose(stdout);
  (void)remove("cli_menu_test_output.txt");
  return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
