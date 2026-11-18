#include "storage.h"
#include "storage_test_support.h"
#include <stdint.h>
#ifndef _WIN32
#include <sys/stat.h>
#endif

static Account account() {
  Account a = {.username = "student001",
               .role = ROLE_CARD_USER,
               .student_id = "251543201"};
  memset(a.password_salt_hex, 'a', PASSWORD_SALT_HEX_LENGTH);
  memset(a.password_hash_hex, 'b', PASSWORD_HASH_HEX_LENGTH);
  return a;
}
static StudentCardRecord record() {
  return (StudentCardRecord){.student_id = "251543201",
                             .name = "张三",
                             .sex = "男",
                             .department = "计算机学院",
                             .class_name = "示例班",
                             .phone = "+86 123-456",
                             .balance_cents = 1250,
                             .status = CARD_STATUS_ACTIVE};
}
static void replace_file(const char *path, const char *original,
                         const char *needle, const char *replacement) {
  const char *where = strstr(original, needle);
  REQUIRE(where);
  size_t size = strlen(original) - strlen(needle) + strlen(replacement);
  char *changed = malloc(size + 1);
  REQUIRE(changed);
  size_t prefix = (size_t)(where - original);
  memcpy(changed, original, prefix);
  strcpy(changed + prefix, replacement);
  strcpy(changed + prefix + strlen(replacement), where + strlen(needle));
  REQUIRE(file_access_write(path, changed, size) == APP_OK);
  free(changed);
}
static void invalid_record_cases(const char *path,
                                 const StudentCardRecord *valid) {
  char *original;
  size_t size;
  REQUIRE(file_access_read(path, &original, &size) == APP_OK);
  const char *bad[][2] = {{"1250", "-1"},
                          {"1250", "1.0"},
                          {"1250", "1e2"},
                          {"1250", "9223372036854775808"},
                          {"1250", "18446744073709551616"},
                          {"1250", "null"},
                          {"1250", "\"1250\""},
                          {"\"active\"", "\"unknown\""},
                          {"\"男\"", "\"未知\""},
                          {"\"张三\"", "\"\""},
                          {"\"张三\"", "\"A\\u0000B\""},
                          {"\"张三\"", "\"\xc0\xaf\""},
                          {"\"张三\"", "\"\\ud800\""},
                          {"\"张三\"", "null"},
                          {"\"张三\"", "42"},
                          {"\"251543201\"", "\"251543202\""},
                          {"\"251543201\"", "\"25154320\""},
                          {"\"+86 123-456\"", "\"123x\""},
                          {"\"phone\"", "\"extra\""},
                          {"\"name\"", "\"sex\""},
                          {"\"name\"", "\"name\\u0000\""},
                          {"\"department\"", "\"name\""},
                          {"{", "{\"extra\":1,"},
                          {"{", "{\"name\":\"张三\","},
                          {"}", ",}"},
                          {"}", "} {}"},
                          {"{", "/*comment*/{"},
                          {"{", "\xef\xbb\xbf{"}};
  for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
    replace_file(path, original, bad[i][0], bad[i][1]);
    StudentCardRecord out = *valid;
    unsigned char before[sizeof(out)];
    memcpy(before, &out, sizeof(out));
    REQUIRE(storage_record_load(valid->student_id, &out) ==
            APP_ERR_INVALID_DATA);
    REQUIRE(!memcmp((const unsigned char *)&out, before, sizeof(out)));
    StudentList list = {};
    REQUIRE(storage_record_list(&list) == APP_ERR_INVALID_DATA && !list.head &&
            !list.tail && !list.size);
  }
  char long_name[NAME_MAX_BYTES + 4];
  long_name[0] = '"';
  memset(long_name + 1, 'x', NAME_MAX_BYTES + 1);
  long_name[NAME_MAX_BYTES + 2] = '"';
  long_name[NAME_MAX_BYTES + 3] = '\0';
  replace_file(path, original, "\"张三\"", long_name);
  StudentCardRecord out;
  REQUIRE(storage_record_load(valid->student_id, &out) == APP_ERR_INVALID_DATA);
  REQUIRE(file_access_write(path, "{", 1) == APP_OK);
  REQUIRE(storage_record_load(valid->student_id, &out) == APP_ERR_INVALID_DATA);
  REQUIRE(file_access_write(path, "", 0) == APP_OK);
  REQUIRE(storage_record_load(valid->student_id, &out) == APP_ERR_INVALID_DATA);
  REQUIRE(file_access_write(path, original, size) == APP_OK);
  free(original);
}
static void invalid_account_cases(const char *path, const Account *valid) {
  char *original;
  size_t size;
  REQUIRE(file_access_read(path, &original, &size) == APP_OK);
  const char *bad[][2] = {{"\"student001\"", "\"another\""},
                          {"\"student001\"", "\"../escape\""},
                          {"\"card_user\"", "\"admin\""},
                          {"\"card_user\"", "\"owner\""},
                          {"\"251543201\"", "\"\""},
                          {"\"password_salt\"", "\"password_hash\""},
                          {"\"password_salt\"", "\"extra\""},
                          {"aaaaaaaa", "AAAAaaaa"},
                          {"bbbbbbbb", "bbbbbbbg"},
                          {"{", "{\"role\":\"card_user\","}};
  for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
    replace_file(path, original, bad[i][0], bad[i][1]);
    Account out = *valid;
    unsigned char before[sizeof(out)];
    memcpy(before, &out, sizeof(out));
    REQUIRE(storage_account_load(valid->username, &out) ==
            APP_ERR_INVALID_DATA);
    REQUIRE(!memcmp((const unsigned char *)&out, before, sizeof(out)));
    size_t count = 123;
    REQUIRE(storage_account_count(&count) == APP_ERR_INVALID_DATA &&
            count == 123);
    AccountList list = {};
    REQUIRE(storage_account_list(&list) == APP_ERR_INVALID_DATA && !list.head &&
            !list.tail && !list.size);
  }
  REQUIRE(file_access_write(path, original, size) == APP_OK);
  free(original);
}
static void test_account_hex_paths_and_case_coexistence(const char *root) {
  Account upper = account(), lower = account(), loaded;
  strcpy(upper.username, "Admin");
  upper.role = ROLE_ADMIN;
  upper.student_id[0] = '\0';
  strcpy(lower.username, "admin");
  lower.role = ROLE_ADMIN;
  lower.student_id[0] = '\0';

  REQUIRE(storage_account_save(&upper) == APP_OK);
  REQUIRE(storage_account_save(&lower) == APP_OK);

  char upper_file[4096], lower_file[4096];
  test_path(root, "accounts/41646d696e.json", upper_file, sizeof(upper_file));
  test_path(root, "accounts/61646d696e.json", lower_file, sizeof(lower_file));

  FileAccessKind kind;
  REQUIRE(file_access_kind(upper_file, &kind) == APP_OK &&
          kind == FILE_ACCESS_REGULAR);
  REQUIRE(file_access_kind(lower_file, &kind) == APP_OK &&
          kind == FILE_ACCESS_REGULAR);

  REQUIRE(storage_account_load("Admin", &loaded) == APP_OK &&
          !strcmp(loaded.username, "Admin"));
  REQUIRE(storage_account_load("admin", &loaded) == APP_OK &&
          !strcmp(loaded.username, "admin"));

  AccountList accounts = {};
  REQUIRE(storage_account_list(&accounts) == APP_OK && accounts.size == 2);
  account_list_clear(&accounts);

  REQUIRE(storage_account_delete("admin") == APP_OK);
  REQUIRE(storage_account_load("admin", &loaded) == APP_ERR_NOT_FOUND);
  REQUIRE(storage_account_load("Admin", &loaded) == APP_OK &&
          !strcmp(loaded.username, "Admin"));
  REQUIRE(storage_account_delete("Admin") == APP_OK);

  /* 验证文件内用户名与路径主键不符时，禁止读取、覆盖或删除。 */
  strcpy(lower.username, "testuser");
  REQUIRE(storage_account_save(&lower) == APP_OK);
  char test_file[4096];
  test_path(root, "accounts/7465737475736572.json", test_file,
            sizeof(test_file));
  char *bytes = nullptr;
  size_t size = 0;
  REQUIRE(file_access_read(test_file, &bytes, &size) == APP_OK);
  char mismatch_file[4096];
  test_path(root, "accounts/616161.json", mismatch_file, sizeof(mismatch_file));
  REQUIRE(file_access_write(mismatch_file, bytes, size) == APP_OK);
  Account mismatch_acc = account();
  strcpy(mismatch_acc.username, "aaa");
  REQUIRE(storage_account_load("aaa", &loaded) == APP_ERR_INVALID_DATA);
  REQUIRE(storage_account_save(&mismatch_acc) == APP_ERR_INVALID_DATA);
  REQUIRE(storage_account_delete("aaa") == APP_ERR_INVALID_DATA);
  free(bytes);
  REQUIRE(file_access_remove(mismatch_file) == APP_OK);
  REQUIRE(storage_account_delete("testuser") == APP_OK);
}

static void test_name_index_check_and_repair(const char *root) {
  NameIndexReport report;
  REQUIRE(storage_name_index_check(nullptr) == APP_ERR_INVALID_ARGUMENT);

  REQUIRE(storage_name_index_check(&report) == APP_OK);

  StudentCardRecord extra = record();
  strcpy(extra.student_id, "251543209");
  strcpy(extra.name, "王五");
  REQUIRE(storage_record_save(&extra) == APP_OK);
  REQUIRE(storage_name_index_check(&report) == APP_OK);
  REQUIRE(report.missing == 1);
  size_t prev_stale = report.stale;

  char stale_path[4096];
  test_path(root, "indexes/name/5b/yg/5L/iJ/251543299", stale_path,
            sizeof(stale_path));
  REQUIRE(file_access_write(stale_path, nullptr, 0) == APP_OK);
  REQUIRE(storage_name_index_check(&report) == APP_OK);
  REQUIRE(report.missing == 1 && report.stale == prev_stale + 1);

  char extra_rec_path[4096];
  test_path(root, "students/25/15/43/2/09.json", extra_rec_path,
            sizeof(extra_rec_path));
  REQUIRE(file_access_write(extra_rec_path, "{broken}", 8) == APP_OK);
  REQUIRE(storage_name_index_check(&report) == APP_ERR_INVALID_DATA);
  REQUIRE(storage_name_index_repair() == APP_ERR_INVALID_DATA);

  REQUIRE(storage_record_save(&extra) == APP_OK);

  char expected_file[4096];
  char expected_dir[4096];
  test_path(root, "indexes/name/54/6L/5L/qU", expected_dir,
            sizeof(expected_dir));
  REQUIRE(file_access_make_directories(expected_dir) == APP_OK);
  test_path(root, "indexes/name/54/6L/5L/qU/251543209", expected_file,
            sizeof(expected_file));
  REQUIRE(file_access_write(expected_file, "non_empty", 9) == APP_OK);
  REQUIRE(storage_name_index_check(&report) == APP_ERR_INVALID_DATA);
  REQUIRE(storage_name_index_repair() == APP_ERR_INVALID_DATA);
  REQUIRE(file_access_remove(expected_file) == APP_OK);

  REQUIRE(storage_name_index_repair() == APP_OK);
  REQUIRE(storage_name_index_check(&report) == APP_OK);
  REQUIRE(report.missing == 0 && report.stale == 0);

  FileAccessKind kind;
  REQUIRE(file_access_kind(expected_file, &kind) == APP_OK &&
          kind == FILE_ACCESS_REGULAR);
  char *cdata;
  size_t csize;
  REQUIRE(file_access_read(expected_file, &cdata, &csize) == APP_OK &&
          csize == 0);
  file_access_free(cdata);

  REQUIRE(file_access_kind(stale_path, &kind) == APP_ERR_NOT_FOUND);

  test_path(root, "students/25/15/43/2/09.json", extra_rec_path,
            sizeof(extra_rec_path));
  REQUIRE(file_access_remove(extra_rec_path) == APP_OK);
  REQUIRE(file_access_remove(expected_file) == APP_OK);
}

int main() {
  char root[4096], path[4096], account_file[4096], record_file[4096],
      index_file[4096];
  test_temp_dir(root, sizeof(root));
  REQUIRE(storage_shutdown() == APP_OK);
  size_t count = 123;
  REQUIRE(storage_account_count(&count) == APP_ERR_INVALID_ARGUMENT &&
          count == 123);
  REQUIRE(storage_init(nullptr) == APP_ERR_INVALID_ARGUMENT);
  REQUIRE(storage_init(root) == APP_OK);
  REQUIRE(storage_init(root) == APP_ERR_INVALID_ARGUMENT);
  test_account_hex_paths_and_case_coexistence(root);
  REQUIRE(storage_account_count(&count) == APP_OK && count == 0);
  StudentList students = {};
  AccountList accounts = {};
  REQUIRE(storage_record_list(&students) == APP_OK && students.size == 0);
  REQUIRE(storage_record_find_by_name("无记录", &students) == APP_OK &&
          students.size == 0);
  StudentCardRecord a = record(), b = record(), loaded;
  strcpy(b.student_id, "251543202");
  REQUIRE(storage_record_save(&a) == APP_OK);
  REQUIRE(storage_record_save(&b) == APP_OK);
  REQUIRE(storage_record_load(a.student_id, &loaded) == APP_OK);
  REQUIRE(!strcmp(loaded.name, a.name) &&
          loaded.balance_cents == a.balance_cents);
  REQUIRE(storage_record_find_by_name(a.name, &students) == APP_OK &&
          students.size == 0);
  REQUIRE(storage_name_index_append(a.name, a.student_id) == APP_OK);
  REQUIRE(storage_name_index_append(a.name, b.student_id) == APP_OK);
  REQUIRE(storage_name_index_append(a.name, a.student_id) == APP_OK);
  REQUIRE(storage_record_find_by_name(a.name, &students) == APP_OK &&
          students.size == 2);
  REQUIRE(student_list_find_by_id(&students, a.student_id));
  student_list_clear(&students);
  REQUIRE(storage_record_list(&students) == APP_OK && students.size == 2);
  student_list_clear(&students);
  Account user = account(), admin = account();
  strcpy(admin.username, "admin");
  admin.role = ROLE_ADMIN;
  admin.student_id[0] = '\0';
  REQUIRE(storage_account_save(&user) == APP_OK);
  REQUIRE(storage_account_save(&admin) == APP_OK);
  REQUIRE(storage_account_count(&count) == APP_OK && count == 2);
  REQUIRE(storage_account_list(&accounts) == APP_OK && accounts.size == 2);
  REQUIRE(account_list_find_by_username(&accounts, user.username));
  account_list_clear(&accounts);
  test_path(root, "accounts/73747564656e74303031.json", account_file,
            sizeof(account_file));
  test_path(root, "students/25/15/43/2/01.json", record_file,
            sizeof(record_file));
  test_path(root, "indexes/name/5b/yg/5L/iJ/251543201", index_file,
            sizeof(index_file));
  FileAccessKind kind;
  REQUIRE(file_access_kind(account_file, &kind) == APP_OK &&
          kind == FILE_ACCESS_REGULAR);
  REQUIRE(file_access_kind(record_file, &kind) == APP_OK &&
          kind == FILE_ACCESS_REGULAR);
  char *content;
  size_t size;
  REQUIRE(file_access_read(index_file, &content, &size) == APP_OK && size == 0);
  free(content);
  invalid_record_cases(record_file, &a);
  invalid_account_cases(account_file, &user);
  /* 非规范路径不参与列表，合法路径中的损坏记录必须报错。 */
  test_path(root, "accounts/not_hex.json", path, sizeof(path));
  REQUIRE(file_access_write(path, "broken", 6) == APP_OK);
  REQUIRE(storage_account_count(&count) == APP_OK && count == 2);
  test_path(root, "students/garbage.json", path, sizeof(path));
  REQUIRE(file_access_write(path, "broken", 6) == APP_OK);
  REQUIRE(storage_record_list(&students) == APP_OK && students.size == 2);
  student_list_clear(&students);
  /* 同名的失效索引和姓名不符的主记录只跳过；损坏主记录不跳过。 */
  REQUIRE(storage_name_index_append(a.name, "999999999") ==
          APP_ERR_NOT_FOUND);
  REQUIRE(storage_name_index_append("李四", a.student_id) ==
          APP_ERR_INVALID_ARGUMENT);
  test_path(root, "indexes/name/5b/yg/5L/iJ/999999999", path, sizeof(path));
  REQUIRE(file_access_write(path, nullptr, 0) == APP_OK);
  strcpy(b.name, "李四");
  REQUIRE(storage_record_save(&b) == APP_OK);
  REQUIRE(storage_record_find_by_name(a.name, &students) == APP_OK &&
          students.size == 1);
  student_list_clear(&students);
  REQUIRE(file_access_remove(path) == APP_OK);
  REQUIRE(file_access_write(record_file, "{}", 2) == APP_OK);
  REQUIRE(storage_record_find_by_name(a.name, &students) ==
              APP_ERR_INVALID_DATA &&
          students.size == 0);
  REQUIRE(storage_record_save(&a) == APP_OK);
  REQUIRE(file_access_write(index_file, "x", 1) == APP_OK);
  REQUIRE(storage_name_index_append(a.name, a.student_id) ==
          APP_ERR_INVALID_DATA);
  REQUIRE(file_access_read(index_file, &content, &size) == APP_OK &&
          size == 1 && content[0] == 'x');
  free(content);
  REQUIRE(file_access_remove(index_file) == APP_OK);
  REQUIRE(file_access_make_directories(index_file) == APP_OK);
  REQUIRE(storage_name_index_append(a.name, a.student_id) ==
          APP_ERR_INVALID_DATA);
  REQUIRE(storage_record_find_by_name(a.name, &students) == APP_OK &&
          students.size == 0);
  test_remove_directory(index_file);
  REQUIRE(storage_name_index_append(a.name, a.student_id) == APP_OK);
  test_path(root, "indexes/name/5b/yg/5L/iJ/invalid", path, sizeof(path));
  REQUIRE(file_access_write(path, nullptr, 0) == APP_OK);
  REQUIRE(storage_record_find_by_name(a.name, &students) == APP_OK &&
          students.size == 1);
  student_list_clear(&students);
  /* 编码末级只有一个字符时仍能定位；姓名目录同名文件应报告 IO。 */
  strcpy(b.name, "ab");
  REQUIRE(storage_record_save(&b) == APP_OK);
  REQUIRE(storage_name_index_append(b.name, b.student_id) == APP_OK);
  test_path(root, "indexes/name/YW/I/251543202", path, sizeof(path));
  REQUIRE(file_access_kind(path, &kind) == APP_OK &&
          kind == FILE_ACCESS_REGULAR);
  REQUIRE(storage_record_find_by_name("ab", &students) == APP_OK &&
          students.size == 1);
  student_list_clear(&students);
  test_path(root, "indexes/name/YW/I", path, sizeof(path));
  test_remove_tree(path);
  REQUIRE(file_access_write(path, "x", 1) == APP_OK);
  REQUIRE(storage_record_find_by_name("ab", &students) == APP_ERR_IO &&
          students.size == 0);
  REQUIRE(file_access_remove(path) == APP_OK);
  REQUIRE(storage_name_index_append(b.name, b.student_id) == APP_OK);
  /* 主记录的直接定位不因其他记录损坏而退化成全表扫描。 */
  test_path(root, "students/25/15/43/2/02.json", path, sizeof(path));
  REQUIRE(file_access_write(path, "{}", 2) == APP_OK);
  REQUIRE(storage_record_load(a.student_id, &loaded) == APP_OK);
  REQUIRE(storage_record_save(&b) == APP_OK);

  /* 最大余额和字符串、冻结状态及不同角色往返。 */
  a.balance_cents = INT64_MAX;
  a.status = CARD_STATUS_FROZEN;
  memset(a.name, 'n', NAME_MAX_BYTES);
  a.name[NAME_MAX_BYTES] = '\0';
  REQUIRE(storage_record_save(&a) == APP_OK);
  REQUIRE(storage_name_index_append(a.name, a.student_id) == APP_OK);
  REQUIRE(storage_record_find_by_name(a.name, &students) == APP_OK &&
          students.size == 1);
  student_list_clear(&students);
  REQUIRE(storage_record_load(a.student_id, &loaded) == APP_OK &&
          loaded.balance_cents == INT64_MAX &&
          loaded.status == CARD_STATUS_FROZEN);
  REQUIRE(storage_shutdown() == APP_OK && storage_shutdown() == APP_OK);
  REQUIRE(storage_init(root) == APP_OK);
  REQUIRE(storage_record_load(a.student_id, &loaded) == APP_OK &&
          loaded.balance_cents == INT64_MAX);
  Account loaded_user;
  REQUIRE(storage_account_load(user.username, &loaded_user) == APP_OK &&
          loaded_user.role == ROLE_CARD_USER);
  REQUIRE(storage_account_delete(user.username) == APP_OK);
  REQUIRE(storage_account_delete(user.username) == APP_ERR_NOT_FOUND);
  REQUIRE(storage_record_load(user.student_id, &loaded) == APP_OK);
  REQUIRE(storage_account_count(&count) == APP_OK && count == 1);
  admin.role = ROLE_CARD_CLERK;
  REQUIRE(storage_account_save(&admin) == APP_OK);
  REQUIRE(storage_account_load(admin.username, &loaded_user) == APP_OK &&
          loaded_user.role == ROLE_CARD_CLERK);
  memset(admin.username, 'u', USERNAME_MAX_BYTES);
  admin.username[USERNAME_MAX_BYTES] = '\0';
  REQUIRE(storage_account_save(&admin) == APP_OK);
  REQUIRE(storage_account_load(admin.username, &loaded_user) == APP_OK);
  a.balance_cents = -1;
  REQUIRE(storage_record_save(&a) == APP_ERR_INVALID_ARGUMENT);
  a = record();
  memset(a.name, 'x', sizeof(a.name));
  REQUIRE(storage_record_save(&a) == APP_ERR_INVALID_ARGUMENT);
  a = record();
  strcpy(a.name, "\xc0\xaf");
  REQUIRE(storage_record_save(&a) == APP_ERR_INVALID_ARGUMENT);
  REQUIRE(storage_account_load("../escape", &loaded_user) ==
          APP_ERR_INVALID_ARGUMENT);
  REQUIRE(storage_record_load("12345678", &loaded) == APP_ERR_INVALID_ARGUMENT);
  REQUIRE(storage_record_find_by_name(nullptr, &students) ==
          APP_ERR_INVALID_ARGUMENT);
  REQUIRE(storage_account_count(nullptr) == APP_ERR_INVALID_ARGUMENT);
  REQUIRE(storage_account_save(nullptr) == APP_ERR_INVALID_ARGUMENT);
  REQUIRE(storage_record_save(nullptr) == APP_ERR_INVALID_ARGUMENT);
#ifndef _WIN32
  if (geteuid() != 0) {
    REQUIRE(chmod(record_file, 0000) == 0);
    REQUIRE(storage_record_load("251543201", &loaded) == APP_ERR_IO);
    REQUIRE(storage_record_list(&students) == APP_ERR_IO && !students.size);
    REQUIRE(chmod(record_file, 0600) == 0);
  }
#endif
  test_name_index_check_and_repair(root);
  /* 初始化后根目录缺失不能误判成零账号或被保存操作悄悄重建。 */
  test_path(root, "accounts", path, sizeof(path));
  test_remove_tree(path);
  count = 999;
  REQUIRE(storage_account_count(&count) == APP_ERR_IO && count == 999);
  REQUIRE(storage_account_save(&admin) == APP_ERR_IO);
  REQUIRE(storage_shutdown() == APP_OK);
  test_remove_tree(root);
  puts("test_storage: passed");
  return 0;
}
