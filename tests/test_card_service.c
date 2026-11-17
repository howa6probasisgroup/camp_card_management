#include "service_test_support.h"
#include "services/card_service.h"

#include <stdint.h>
#include <string.h>

#define CHECK(condition) service_test_check((condition), #condition, __LINE__)
#define CHECK_RESULT(actual, expected)                                         \
  service_test_check_result((actual), (expected), #actual, __LINE__)

static StudentCardRecord make_record(const char *student_id, const char *name,
                                     const int64_t balance,
                                     const CardStatus status) {
  StudentCardRecord record = {0};
  memcpy(record.student_id, student_id, strlen(student_id) + 1);
  memcpy(record.name, name, strlen(name) + 1);
  memcpy(record.sex, "男", sizeof "男");
  memcpy(record.department, "计算机学院", sizeof "计算机学院");
  memcpy(record.class_name, "示例班", sizeof "示例班");
  memcpy(record.phone, "13800000000", sizeof "13800000000");
  record.balance_cents = balance;
  record.status = status;
  return record;
}

static StudentProfileInput make_profile(const char *student_id,
                                        const char *name) {
  StudentProfileInput profile = {0};
  memcpy(profile.student_id, student_id, strlen(student_id) + 1);
  memcpy(profile.name, name, strlen(name) + 1);
  memcpy(profile.sex, "男", sizeof "男");
  memcpy(profile.department, "计算机学院", sizeof "计算机学院");
  memcpy(profile.class_name, "示例班", sizeof "示例班");
  memcpy(profile.phone, "13800000000", sizeof "13800000000");
  return profile;
}

static void seed_card_environment() {
  fake_storage_reset();
  CHECK(fake_storage_seed_account_with_password("admin", "adminpass",
                                                ROLE_ADMIN, "") == APP_OK);
  CHECK(fake_storage_seed_account_with_password("clerk", "clerkpass",
                                                ROLE_CARD_CLERK, "") == APP_OK);
  CHECK(fake_storage_seed_account_with_password(
            "user001", "userpass", ROLE_CARD_USER, "251543201") == APP_OK);
  CHECK(fake_storage_seed_account_with_password(
            "user003", "userpass", ROLE_CARD_USER, "251543203") == APP_OK);

  const StudentCardRecord active =
      make_record("251543201", "张三", 1000, CARD_STATUS_ACTIVE);
  const StudentCardRecord same_name =
      make_record("251543202", "张三", 0, CARD_STATUS_ACTIVE);
  const StudentCardRecord frozen =
      make_record("251543203", "李四", 1000, CARD_STATUS_FROZEN);
  const StudentCardRecord overflow =
      make_record("251543204", "王五", INT64_MAX, CARD_STATUS_ACTIVE);
  CHECK(fake_storage_seed_record(&active) == APP_OK);
  CHECK(fake_storage_seed_record(&same_name) == APP_OK);
  CHECK(fake_storage_seed_record(&frozen) == APP_OK);
  CHECK(fake_storage_seed_record(&overflow) == APP_OK);
}

static void test_card_issue_validation_and_partial_failure() {
  seed_card_environment();
  const Account *admin = fake_storage_find_account("admin");
  const Account *clerk = fake_storage_find_account("clerk");
  const Account *user = fake_storage_find_account("user001");
  CHECK(admin != nullptr);
  CHECK(clerk != nullptr);
  CHECK(user != nullptr);

  StudentProfileInput profile = make_profile("251543205", "赵六");
  CHECK(card_issue(nullptr, &profile) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_issue(clerk, nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_issue(user, &profile) == APP_ERR_PERMISSION_DENIED);
  CHECK(card_issue(admin, &profile) == APP_ERR_PERMISSION_DENIED);

  profile.student_id[0] = 'x';
  CHECK(card_issue(clerk, &profile) == APP_ERR_INVALID_ARGUMENT);
  profile = make_profile("251543205", "");
  CHECK(card_issue(clerk, &profile) == APP_ERR_INVALID_ARGUMENT);
  profile = make_profile("251543205", "赵六");
  memcpy(profile.sex, "未知", sizeof "未知");
  CHECK(card_issue(clerk, &profile) == APP_ERR_INVALID_ARGUMENT);
  profile = make_profile("251543205", "赵六");
  profile.department[0] = '\0';
  CHECK(card_issue(clerk, &profile) == APP_ERR_INVALID_ARGUMENT);
  profile = make_profile("251543205", "赵六");
  profile.class_name[0] = '\0';
  CHECK(card_issue(clerk, &profile) == APP_ERR_INVALID_ARGUMENT);
  profile = make_profile("251543205", "赵六");
  memcpy(profile.phone, "123_456", sizeof "123_456");
  CHECK(card_issue(clerk, &profile) == APP_ERR_INVALID_ARGUMENT);
  profile = make_profile("251543205", "赵六");
  profile.name[0] = (char)0xc3;
  profile.name[1] = (char)0x28;
  profile.name[2] = '\0';
  CHECK(card_issue(clerk, &profile) == APP_ERR_INVALID_ARGUMENT);
  profile = make_profile("251543205", "赵六");
  memset(profile.name, 'a', NAME_MAX_BYTES + 1);
  CHECK(card_issue(clerk, &profile) == APP_ERR_INVALID_ARGUMENT);
  profile = make_profile("251543207", "赵六");
  memset(profile.name, 'a', NAME_MAX_BYTES);
  profile.name[NAME_MAX_BYTES] = '\0';
  CHECK(card_issue(clerk, &profile) == APP_OK);

  profile = make_profile("251543201", "重复");
  size_t save_calls = fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE);
  const size_t index_calls =
      fake_storage_call_count(FAKE_STORAGE_NAME_INDEX_APPEND);
  CHECK(card_issue(clerk, &profile) == APP_ERR_ALREADY_EXISTS);
  CHECK(fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE) == save_calls);
  CHECK(fake_storage_call_count(FAKE_STORAGE_NAME_INDEX_APPEND) == index_calls);

  profile = make_profile("251543205", "赵六");
  CHECK(card_issue(clerk, &profile) == APP_OK);
  const StudentCardRecord *issued = fake_storage_find_record("251543205");
  CHECK(issued != nullptr);
  if (issued != nullptr) {
    CHECK(issued->balance_cents == 0);
    CHECK(issued->status == CARD_STATUS_ACTIVE);
    CHECK(strcmp(issued->name, "赵六") == 0);
  }

  profile = make_profile("251543206", "钱七");
  save_calls = fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE);
  fake_storage_fail_next(FAKE_STORAGE_NAME_INDEX_APPEND, APP_ERR_IO);
  CHECK(card_issue(clerk, &profile) == APP_ERR_IO);
  CHECK(fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE) == save_calls + 1);
  CHECK(fake_storage_find_record("251543206") != nullptr);
}

static void test_card_queries() {
  seed_card_environment();
  const Account *admin = fake_storage_find_account("admin");
  const Account *clerk = fake_storage_find_account("clerk");
  const Account *user = fake_storage_find_account("user001");
  CHECK(admin != nullptr);
  CHECK(clerk != nullptr);
  CHECK(user != nullptr);

  StudentCardRecord record = {0};
  CHECK(card_get_by_id(nullptr, "251543201", &record) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(card_get_by_id(clerk, nullptr, &record) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_get_by_id(clerk, "251543201", nullptr) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(card_get_by_id(clerk, "bad-id", &record) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_get_by_id(clerk, "251543299", &record) == APP_ERR_NOT_FOUND);
  CHECK(card_get_by_id(clerk, "251543202", &record) == APP_OK);
  CHECK(strcmp(record.name, "张三") == 0);
  CHECK(card_get_by_id(user, "251543201", &record) == APP_OK);
  CHECK(strcmp(record.student_id, "251543201") == 0);
  CHECK(card_get_by_id(user, "251543202", &record) ==
        APP_ERR_PERMISSION_DENIED);
  CHECK(card_get_by_id(admin, "251543201", &record) ==
        APP_ERR_PERMISSION_DENIED);
  CHECK_RESULT(card_get_by_id(admin, "bad-id", &record),
               APP_ERR_PERMISSION_DENIED);

  const StudentCardRecord broken =
      make_record("251543299", "损坏", -1, CARD_STATUS_ACTIVE);
  CHECK(fake_storage_seed_record(&broken) == APP_OK);
  memcpy(record.student_id, "unchanged", sizeof "unchanged");
  CHECK_RESULT(card_get_by_id(clerk, "251543299", &record),
               APP_ERR_INVALID_DATA);
  CHECK(strcmp(record.student_id, "unchanged") == 0);

  StudentList results;
  student_list_init(&results);
  CHECK(card_find_by_name(nullptr, "张三", &results) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(card_find_by_name(user, "张三", &results) == APP_ERR_PERMISSION_DENIED);
  CHECK(card_find_by_name(clerk, nullptr, &results) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(card_find_by_name(clerk, "张三", nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_find_by_name(clerk, "", &results) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_find_by_name(clerk, "不存在", &results) == APP_OK);
  CHECK(results.size == 0);
  CHECK(card_find_by_name(clerk, "张三", &results) == APP_OK);
  CHECK(results.size == 2);
  CHECK(student_list_find_by_id(&results, "251543201") != nullptr);
  CHECK(student_list_find_by_id(&results, "251543202") != nullptr);
  student_list_clear(&results);

  student_list_init(&results);
  CHECK_RESULT(card_find_by_name(clerk, "损坏", &results),
               APP_ERR_INVALID_DATA);
  CHECK(results.size == 0);
  student_list_clear(&results);

  student_list_init(&results);
  fake_storage_fail_next(FAKE_STORAGE_RECORD_FIND_BY_NAME, APP_ERR_IO);
  CHECK(card_find_by_name(clerk, "张三", &results) == APP_ERR_IO);
  CHECK(results.size == 0);
  student_list_clear(&results);

  memset(&record, 0, sizeof record);
}

static void test_phone_and_money_operations() {
  seed_card_environment();
  const Account *clerk = fake_storage_find_account("clerk");
  const Account *user = fake_storage_find_account("user001");
  const Account *frozen = fake_storage_find_account("user003");
  CHECK(clerk != nullptr);
  CHECK(user != nullptr);
  CHECK(frozen != nullptr);

  CHECK(card_update_own_phone(nullptr, "") == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_update_own_phone(clerk, "") == APP_ERR_PERMISSION_DENIED);
  CHECK(card_update_own_phone(user, nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_update_own_phone(user, "123_456") == APP_ERR_INVALID_ARGUMENT);
  char maximum_phone[PHONE_MAX_BYTES + 1];
  memset(maximum_phone, '1', PHONE_MAX_BYTES);
  maximum_phone[PHONE_MAX_BYTES] = '\0';
  CHECK(card_update_own_phone(user, maximum_phone) == APP_OK);
  char overlong_phone[PHONE_MAX_BYTES + 2];
  memset(overlong_phone, '1', PHONE_MAX_BYTES + 1);
  overlong_phone[PHONE_MAX_BYTES + 1] = '\0';
  CHECK(card_update_own_phone(user, overlong_phone) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(card_update_own_phone(user, "+86-123 456") == APP_OK);
  const StudentCardRecord *updated = fake_storage_find_record("251543201");
  CHECK(updated != nullptr);
  if (updated != nullptr) {
    CHECK(strcmp(updated->phone, "+86-123 456") == 0);
  }
  CHECK(card_update_own_phone(frozen, "") == APP_OK);
  const StudentCardRecord *updated_frozen =
      fake_storage_find_record("251543203");
  CHECK(updated_frozen != nullptr);
  if (updated_frozen != nullptr) {
    CHECK(updated_frozen->phone[0] == '\0');
  }

  int64_t balance = 777;
  size_t save_calls = fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE);
  CHECK(card_recharge(nullptr, 100, &balance) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_recharge(clerk, 100, &balance) == APP_ERR_PERMISSION_DENIED);
  CHECK(card_recharge(user, 100, nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_recharge(user, 0, &balance) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_recharge(user, -1, &balance) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_recharge(frozen, 100, &balance) == APP_ERR_CARD_FROZEN);
  CHECK(fake_storage_seed_account_with_password(
            "user004", "userpass", ROLE_CARD_USER, "251543204") == APP_OK);
  const Account *overflow_account = fake_storage_find_account("user004");
  CHECK(overflow_account != nullptr);
  CHECK(card_recharge(overflow_account, 1, &balance) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE) == save_calls);
  CHECK(balance == 777);

  CHECK(card_recharge(user, 250, &balance) == APP_OK);
  CHECK(balance == 1250);

  save_calls = fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE);
  fake_storage_fail_next(FAKE_STORAGE_RECORD_SAVE, APP_ERR_IO);
  balance = 777;
  CHECK(card_recharge(user, 50, &balance) == APP_ERR_IO);
  CHECK(balance == 777);
  CHECK(fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE) == save_calls + 1);

  balance = 777;
  CHECK(card_consume(nullptr, 100, &balance) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_consume(clerk, 100, &balance) == APP_ERR_PERMISSION_DENIED);
  CHECK(card_consume(user, 100, nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_consume(user, 0, &balance) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_consume(user, -1, &balance) == APP_ERR_INVALID_ARGUMENT);
  CHECK(card_consume(frozen, 100, &balance) == APP_ERR_CARD_FROZEN);
  CHECK(card_consume(user, 5000, &balance) == APP_ERR_INSUFFICIENT_BALANCE);
  CHECK(balance == 777);

  CHECK(card_consume(user, 250, &balance) == APP_OK);
  CHECK(balance == 1000);

  fake_storage_fail_next(FAKE_STORAGE_RECORD_SAVE, APP_ERR_IO);
  balance = 777;
  CHECK(card_consume(user, 50, &balance) == APP_ERR_IO);
  CHECK(balance == 777);
}

static void test_card_status_changes() {
  seed_card_environment();
  const Account *clerk = fake_storage_find_account("clerk");
  const Account *user = fake_storage_find_account("user001");
  CHECK(clerk != nullptr);
  CHECK(user != nullptr);

  CHECK(card_set_status(nullptr, "251543201", CARD_STATUS_FROZEN) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(card_set_status(user, "251543201", CARD_STATUS_FROZEN) ==
        APP_ERR_PERMISSION_DENIED);
  CHECK(card_set_status(clerk, nullptr, CARD_STATUS_FROZEN) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(card_set_status(clerk, "bad-id", CARD_STATUS_FROZEN) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(card_set_status(clerk, "251543201", (CardStatus)99) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(card_set_status(clerk, "251543299", CARD_STATUS_FROZEN) ==
        APP_ERR_NOT_FOUND);

  size_t save_calls = fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE);
  CHECK(card_set_status(clerk, "251543201", CARD_STATUS_ACTIVE) == APP_OK);
  CHECK(fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE) == save_calls);

  CHECK(card_set_status(clerk, "251543201", CARD_STATUS_FROZEN) == APP_OK);
  const StudentCardRecord *frozen = fake_storage_find_record("251543201");
  CHECK(frozen != nullptr);
  if (frozen != nullptr) {
    CHECK(frozen->status == CARD_STATUS_FROZEN);
  }

  save_calls = fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE);
  CHECK(card_set_status(clerk, "251543201", CARD_STATUS_FROZEN) == APP_OK);
  CHECK(fake_storage_call_count(FAKE_STORAGE_RECORD_SAVE) == save_calls);

  CHECK(card_set_status(clerk, "251543201", CARD_STATUS_ACTIVE) == APP_OK);
  const StudentCardRecord *active = fake_storage_find_record("251543201");
  CHECK(active != nullptr);
  if (active != nullptr) {
    CHECK(active->status == CARD_STATUS_ACTIVE);
  }
}

int main() {
  test_card_issue_validation_and_partial_failure();
  test_card_queries();
  test_phone_and_money_operations();
  test_card_status_changes();
  return service_test_finish("test_card_service");
}
