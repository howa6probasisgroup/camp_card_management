#include "service_test_support.h"
#include "services/account_service.h"

#include "password.h"

#include <string.h>

#define CHECK(condition) service_test_check((condition), #condition, __LINE__)

static StudentCardRecord make_record(const char *student_id, const char *name) {
  StudentCardRecord record = {0};
  memcpy(record.student_id, student_id, strlen(student_id) + 1);
  memcpy(record.name, name, strlen(name) + 1);
  memcpy(record.sex, "男", sizeof "男");
  memcpy(record.department, "计算机学院", sizeof "计算机学院");
  memcpy(record.class_name, "示例班", sizeof "示例班");
  record.status = CARD_STATUS_ACTIVE;
  return record;
}

static void seed_common_accounts() {
  fake_storage_reset();
  CHECK(fake_storage_seed_account_with_password("admin", "adminpass",
                                                ROLE_ADMIN, "") == APP_OK);
  CHECK(fake_storage_seed_account_with_password("clerk", "clerkpass",
                                                ROLE_CARD_CLERK, "") == APP_OK);
  CHECK(fake_storage_seed_account_with_password(
            "user001", "userpass", ROLE_CARD_USER, "251543201") == APP_OK);
  const StudentCardRecord record = make_record("251543201", "张三");
  CHECK(fake_storage_seed_record(&record) == APP_OK);
}

static void test_account_creation_and_permissions() {
  seed_common_accounts();
  const Account *admin = fake_storage_find_account("admin");
  const Account *user = fake_storage_find_account("user001");
  CHECK(admin != nullptr);
  CHECK(user != nullptr);

  CHECK(account_create(nullptr, "newuser", "newpass1", ROLE_CARD_CLERK, "") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(account_create(user, "newuser", "newpass1", ROLE_CARD_CLERK, "") ==
        APP_ERR_PERMISSION_DENIED);
  CHECK(account_create(admin, nullptr, "newpass1", ROLE_CARD_CLERK, "") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(account_create(admin, "newuser", nullptr, ROLE_CARD_CLERK, "") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(account_create(admin, "newuser", "newpass1", ROLE_CARD_CLERK,
                       nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_create(admin, "bad name", "newpass1", ROLE_CARD_CLERK, "") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(account_create(admin, "newuser", "short", ROLE_CARD_CLERK, "") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(account_create(admin, "newuser", "newpass1", (UserRole)99, "") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(account_create(admin, "newuser", "newpass1", ROLE_CARD_USER, "") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(account_create(admin, "newuser", "newpass1", ROLE_CARD_CLERK,
                       "251543201") == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_create(admin, "clerk", "newpass1", ROLE_CARD_CLERK, "") ==
        APP_ERR_ALREADY_EXISTS);
  CHECK(account_create(admin, "newuser", "newpass1", ROLE_CARD_USER,
                       "251543299") == APP_ERR_NOT_FOUND);

  CHECK(account_create(admin, "clerk2", "clerkpass2", ROLE_CARD_CLERK, "") ==
        APP_OK);
  const Account *clerk = fake_storage_find_account("clerk2");
  CHECK(clerk != nullptr);
  if (clerk != nullptr) {
    CHECK(clerk->role == ROLE_CARD_CLERK);
    CHECK(clerk->student_id[0] == '\0');
    CHECK(password_verify("clerkpass2", clerk->password_salt_hex,
                          clerk->password_hash_hex) == APP_OK);
  }

  CHECK(account_create(admin, "user002", "userpass2", ROLE_CARD_USER,
                       "251543201") == APP_OK);
  const Account *created_user = fake_storage_find_account("user002");
  CHECK(created_user != nullptr);
  if (created_user != nullptr) {
    CHECK(created_user->role == ROLE_CARD_USER);
    CHECK(strcmp(created_user->student_id, "251543201") == 0);
  }

  char maximum_username[USERNAME_MAX_BYTES + 1];
  memset(maximum_username, 'u', USERNAME_MAX_BYTES);
  maximum_username[USERNAME_MAX_BYTES] = '\0';
  CHECK(account_create(admin, maximum_username, "maxpass11", ROLE_CARD_CLERK,
                       "") == APP_OK);

  char overlong_username[USERNAME_MAX_BYTES + 2];
  memset(overlong_username, 'u', USERNAME_MAX_BYTES + 1);
  overlong_username[USERNAME_MAX_BYTES + 1] = '\0';
  CHECK(account_create(admin, overlong_username, "maxpass11", ROLE_CARD_CLERK,
                       "") == APP_ERR_INVALID_ARGUMENT);
}

static void test_account_query_list_and_delete() {
  seed_common_accounts();
  const Account *admin = fake_storage_find_account("admin");
  const Account *user = fake_storage_find_account("user001");
  CHECK(admin != nullptr);
  CHECK(user != nullptr);

  Account fetched = {0};
  CHECK(account_get(nullptr, "clerk", &fetched) == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_get(user, "clerk", &fetched) == APP_ERR_PERMISSION_DENIED);
  CHECK(account_get(admin, nullptr, &fetched) == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_get(admin, "clerk", nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_get(admin, "bad name", &fetched) == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_get(admin, "missing", &fetched) == APP_ERR_NOT_FOUND);
  CHECK(account_get(admin, "clerk", &fetched) == APP_OK);
  CHECK(strcmp(fetched.username, "clerk") == 0);
  CHECK(fetched.role == ROLE_CARD_CLERK);

  AccountList list;
  account_list_init(&list);
  CHECK(account_list(nullptr, &list) == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_list(user, &list) == APP_ERR_PERMISSION_DENIED);
  CHECK(account_list(admin, nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_list(admin, &list) == APP_OK);
  CHECK(list.size == 3);
  CHECK(account_list_find_by_username(&list, "admin") != nullptr);
  CHECK(account_list_find_by_username(&list, "user001") != nullptr);
  account_list_clear(&list);

  CHECK(account_delete(nullptr, "clerk") == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_delete(user, "clerk") == APP_ERR_PERMISSION_DENIED);
  CHECK(account_delete(admin, nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_delete(admin, "bad name") == APP_ERR_INVALID_ARGUMENT);
  CHECK(account_delete(admin, "admin") == APP_ERR_PERMISSION_DENIED);
  CHECK(account_delete(admin, "missing") == APP_ERR_NOT_FOUND);
  CHECK(account_delete(admin, "clerk") == APP_OK);
  CHECK(fake_storage_find_account("clerk") == nullptr);
  CHECK(fake_storage_find_record("251543201") != nullptr);

  memset(&fetched, 0, sizeof fetched);
}

static void test_account_storage_errors() {
  fake_storage_reset();
  CHECK(fake_storage_seed_account_with_password("admin", "adminpass",
                                                ROLE_ADMIN, "") == APP_OK);
  const Account *admin = fake_storage_find_account("admin");
  const StudentCardRecord record = make_record("251543201", "张三");
  CHECK(fake_storage_seed_record(&record) == APP_OK);

  fake_storage_fail_next(FAKE_STORAGE_ACCOUNT_LOAD, APP_ERR_IO);
  CHECK(account_create(admin, "clerk2", "clerkpass2", ROLE_CARD_CLERK, "") ==
        APP_ERR_IO);

  fake_storage_fail_next(FAKE_STORAGE_RECORD_LOAD, APP_ERR_IO);
  CHECK(account_create(admin, "user002", "userpass2", ROLE_CARD_USER,
                       "251543201") == APP_ERR_IO);

  fake_storage_fail_next(FAKE_STORAGE_ACCOUNT_SAVE, APP_ERR_IO);
  CHECK(account_create(admin, "clerk2", "clerkpass2", ROLE_CARD_CLERK, "") ==
        APP_ERR_IO);
  CHECK(fake_storage_find_account("clerk2") == nullptr);

  Account fetched = {0};
  fake_storage_fail_next(FAKE_STORAGE_ACCOUNT_LOAD, APP_ERR_INVALID_DATA);
  CHECK(account_get(admin, "admin", &fetched) == APP_ERR_INVALID_DATA);

  AccountList list;
  account_list_init(&list);
  fake_storage_fail_next(FAKE_STORAGE_ACCOUNT_LIST, APP_ERR_IO);
  CHECK(account_list(admin, &list) == APP_ERR_IO);
  CHECK(list.size == 0);
  account_list_clear(&list);

  fake_storage_fail_next(FAKE_STORAGE_ACCOUNT_DELETE, APP_ERR_IO);
  CHECK(account_delete(admin, "missing") == APP_ERR_IO);

  Account broken = {0};
  memcpy(broken.username, "broken", sizeof "broken");
  broken.role = ROLE_ADMIN;
  CHECK(fake_storage_seed_account(&broken) == APP_OK);
  memcpy(fetched.username, "unchanged", sizeof "unchanged");
  CHECK(account_get(admin, "broken", &fetched) == APP_ERR_INVALID_DATA);
  CHECK(strcmp(fetched.username, "unchanged") == 0);

  account_list_init(&list);
  CHECK(account_list(admin, &list) == APP_ERR_INVALID_DATA);
  CHECK(list.size == 0);
  account_list_clear(&list);

  memset(&fetched, 0, sizeof fetched);
  memset(&broken, 0, sizeof broken);
}

int main() {
  test_account_creation_and_permissions();
  test_account_query_list_and_delete();
  test_account_storage_errors();
  return service_test_finish("test_account_service");
}
