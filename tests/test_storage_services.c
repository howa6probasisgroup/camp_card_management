#include "services/account_service.h"
#include "services/auth_service.h"
#include "services/card_service.h"
#include "storage.h"
#include "storage_test_support.h"

/* 链接真实文件存储与密码模块，覆盖跨模块调用和关闭后重新读取。 */
int main(void) {
  char root[4096], path[4096];
  test_temp_dir(root, sizeof(root));
  REQUIRE(storage_init(root) == APP_OK);
  bool needed = false;
  REQUIRE(auth_needs_bootstrap(&needed) == APP_OK && needed);
  REQUIRE(auth_bootstrap_admin("admin", "demo-pass-123") == APP_OK);
  REQUIRE(auth_bootstrap_admin("another", "demo-pass-123") ==
          APP_ERR_ALREADY_EXISTS);
  Account admin, clerk, user;
  REQUIRE(auth_login("admin", "demo-pass-123", &admin) == APP_OK);
  REQUIRE(account_create(&admin, "clerk", "demo-pass-123", ROLE_CARD_CLERK,
                         "") == APP_OK);
  REQUIRE(auth_login("clerk", "demo-pass-123", &clerk) == APP_OK);
  StudentProfileInput profile = {.student_id = "251543201",
                                 .name = "示例学生",
                                 .sex = "其他",
                                 .department = "示例学院",
                                 .class_name = "示例班",
                                 .phone = ""};
  REQUIRE(card_issue(&clerk, &profile) == APP_OK);
  REQUIRE(card_issue(&clerk, &profile) == APP_ERR_ALREADY_EXISTS);
  REQUIRE(account_create(&admin, "student", "demo-pass-123", ROLE_CARD_USER,
                         profile.student_id) == APP_OK);
  REQUIRE(auth_login("student", "demo-pass-123", &user) == APP_OK);
  int64_t balance = -1;
  REQUIRE(card_recharge(&user, 10000, &balance) == APP_OK && balance == 10000);
  REQUIRE(card_consume(&user, 1250, &balance) == APP_OK && balance == 8750);
  REQUIRE(card_consume(&user, 10000, &balance) ==
              APP_ERR_INSUFFICIENT_BALANCE &&
          balance == 8750);
  REQUIRE(card_update_own_phone(&user, "123-456") == APP_OK);
  REQUIRE(card_set_status(&clerk, profile.student_id, CARD_STATUS_FROZEN) ==
          APP_OK);
  REQUIRE(card_recharge(&user, 100, &balance) == APP_ERR_CARD_FROZEN &&
          balance == 8750);
  REQUIRE(auth_change_password(&user, "demo-pass-123", "new-demo-456") ==
          APP_OK);
  REQUIRE(storage_shutdown() == APP_OK);
  REQUIRE(storage_init(root) == APP_OK);
  REQUIRE(auth_needs_bootstrap(&needed) == APP_OK && !needed);
  REQUIRE(auth_login("student", "demo-pass-123", &user) == APP_ERR_AUTH_FAILED);
  REQUIRE(auth_login("student", "new-demo-456", &user) == APP_OK);
  StudentCardRecord record;
  REQUIRE(card_get_by_id(&user, profile.student_id, &record) == APP_OK);
  REQUIRE(record.balance_cents == 8750 && record.status == CARD_STATUS_FROZEN &&
          !strcmp(record.phone, "123-456"));
  StudentList found = {0};
  REQUIRE(card_find_by_name(&clerk, profile.name, &found) == APP_OK &&
          found.size == 1);
  student_list_clear(&found);
  REQUIRE(card_set_status(&clerk, profile.student_id, CARD_STATUS_ACTIVE) ==
          APP_OK);
  REQUIRE(card_consume(&user, 8750, &balance) == APP_OK && balance == 0);
  REQUIRE(account_delete(&admin, "student") == APP_OK);
  REQUIRE(storage_record_load(profile.student_id, &record) == APP_OK &&
          record.balance_cents == 0);
  test_path(root, "accounts/YWRtaW4.json", path, sizeof(path));
  REQUIRE(file_access_write(path, "{}", 2) == APP_OK);
  needed = false;
  REQUIRE(auth_needs_bootstrap(&needed) == APP_ERR_INVALID_DATA && !needed);
  REQUIRE(auth_bootstrap_admin("other", "demo-pass-123") ==
          APP_ERR_INVALID_DATA);
  REQUIRE(storage_shutdown() == APP_OK);
  test_remove_tree(root);
  puts("test_storage_services: passed");
  return 0;
}
