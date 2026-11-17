#ifndef CAMP_CARD_TESTS_SERVICE_TEST_SUPPORT_H
#define CAMP_CARD_TESTS_SERVICE_TEST_SUPPORT_H

#include "data_structures.h"

#include <stddef.h>

typedef enum {
  FAKE_STORAGE_ACCOUNT_COUNT = 0,
  FAKE_STORAGE_ACCOUNT_LOAD,
  FAKE_STORAGE_ACCOUNT_SAVE,
  FAKE_STORAGE_ACCOUNT_DELETE,
  FAKE_STORAGE_ACCOUNT_LIST,
  FAKE_STORAGE_RECORD_LOAD,
  FAKE_STORAGE_RECORD_SAVE,
  FAKE_STORAGE_RECORD_LIST,
  FAKE_STORAGE_RECORD_FIND_BY_NAME,
  FAKE_STORAGE_NAME_INDEX_APPEND,
  FAKE_STORAGE_OPERATION_COUNT
} FakeStorageOperation;

void service_test_check(bool condition, const char *expression, int line);
void service_test_check_result(AppResult actual, AppResult expected,
                               const char *expression, int line);
int service_test_finish(const char *test_name);

void fake_storage_reset();
void fake_storage_fail_next(FakeStorageOperation operation, AppResult result);
size_t fake_storage_call_count(FakeStorageOperation operation);

AppResult fake_storage_seed_account(const Account *account);
AppResult fake_storage_seed_account_with_password(const char *username,
                                                  const char *password,
                                                  UserRole role,
                                                  const char *student_id);
AppResult fake_storage_seed_record(const StudentCardRecord *record);

const Account *fake_storage_find_account(const char *username);
const StudentCardRecord *fake_storage_find_record(const char *student_id);
size_t fake_storage_account_count_seeded();
size_t fake_storage_record_count_seeded();

#endif // CAMP_CARD_TESTS_SERVICE_TEST_SUPPORT_H
