#include "data_structures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 只验证业务层依赖的链表契约：模型副本、按业务键定位与删除、字符串去重。

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

static void set_text(char *destination, const size_t capacity,
                     const char *source) {
  const size_t length = strlen(source);
  if (length >= capacity) {
    fprintf(stderr, "test setup error: text does not fit buffer\n");
    exit(EXIT_FAILURE);
  }
  memcpy(destination, source, length + 1);
}

static StudentCardRecord make_student(const char *student_id, const char *name,
                                      const int64_t balance_cents,
                                      const CardStatus status) {
  StudentCardRecord record;
  memset(&record, 0, sizeof record);
  set_text(record.student_id, sizeof record.student_id, student_id);
  set_text(record.name, sizeof record.name, name);
  set_text(record.sex, sizeof record.sex, "男");
  set_text(record.department, sizeof record.department, "计算机学院");
  set_text(record.class_name, sizeof record.class_name, "24154B1");
  set_text(record.phone, sizeof record.phone, "13800000000");
  record.balance_cents = balance_cents;
  record.status = status;
  return record;
}

static Account make_account(const char *username, const UserRole role,
                            const char *student_id) {
  Account account;
  memset(&account, 0, sizeof account);
  set_text(account.username, sizeof account.username, username);
  set_text(account.password_salt_hex, sizeof account.password_salt_hex,
           "00112233445566778899aabbccddeeff");
  set_text(account.password_hash_hex, sizeof account.password_hash_hex,
           "0123456789abcdef0123456789abcdef"
           "0123456789abcdef0123456789abcdef");
  account.role = role;
  set_text(account.student_id, sizeof account.student_id, student_id);
  return account;
}

static void test_student_records_are_queryable_copies() {
  StudentList list;
  student_list_init(&list);
  StudentCardRecord original =
      make_student("251543201", "张三", 1200, CARD_STATUS_ACTIVE);
  const StudentCardRecord frozen =
      make_student("251543202", "李四", 0, CARD_STATUS_FROZEN);

  CHECK(student_list_append(&list, &original));
  CHECK(student_list_append(&list, &frozen));
  CHECK(student_list_count(&list) == 2);

  set_text(original.name, sizeof original.name, "赵六");
  original.balance_cents = 9999;
  const StudentCardRecord *found = student_list_find_by_id(&list, "251543201");
  CHECK(found != nullptr);
  if (found != nullptr) {
    CHECK(strcmp(found->name, "张三") == 0);
    CHECK(found->balance_cents == 1200);
  }

  CHECK(student_list_remove_by_id(&list, "251543201"));
  CHECK(student_list_find_by_id(&list, "251543201") == nullptr);
  found = student_list_find_by_id(&list, "251543202");
  CHECK(found != nullptr && found->status == CARD_STATUS_FROZEN);
  CHECK(student_list_count(&list) == 1);

  student_list_clear(&list);
  CHECK(student_list_count(&list) == 0);
}

static void test_account_roles_are_queryable_by_username() {
  AccountList list;
  account_list_init(&list);
  const Account admin = make_account("admin", ROLE_ADMIN, "");
  const Account user = make_account("user001", ROLE_CARD_USER, "251543201");

  CHECK(account_list_append(&list, &admin));
  CHECK(account_list_append(&list, &user));

  const Account *found = account_list_find_by_username(&list, "user001");
  CHECK(found != nullptr && found->role == ROLE_CARD_USER);
  CHECK(found != nullptr && strcmp(found->student_id, "251543201") == 0);

  CHECK(account_list_remove_by_username(&list, "admin"));
  CHECK(account_list_find_by_username(&list, "admin") == nullptr);

  account_list_clear(&list);
  CHECK(account_list_count(&list) == 0);
}

static void test_string_ids_are_unique_copies() {
  StringList list;
  string_list_init(&list);
  char student_id[STUDENT_ID_LENGTH + 1];
  set_text(student_id, sizeof student_id, "251543201");

  CHECK(string_list_append_unique(&list, student_id));
  CHECK(string_list_append_unique(&list, student_id));
  CHECK(string_list_count(&list) == 1);

  set_text(student_id, sizeof student_id, "251543202");
  CHECK(string_list_contains(&list, "251543201"));
  CHECK(string_list_contains(&list, student_id) == false);

  string_list_clear(&list);
  CHECK(string_list_count(&list) == 0);
}

int main() {
  test_student_records_are_queryable_copies();
  test_account_roles_are_queryable_by_username();
  test_string_ids_are_unique_copies();

  if (g_failures == 0) {
    printf("test_data_structures: %zu core checks passed\n", g_checks);
    return EXIT_SUCCESS;
  }
  fprintf(stderr, "test_data_structures: %zu of %zu checks failed\n",
          g_failures, g_checks);
  return EXIT_FAILURE;
}
