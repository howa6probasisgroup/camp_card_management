#include "service_test_support.h"

#include "password.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>

enum { FAKE_STORAGE_MAX_ACCOUNTS = 32, FAKE_STORAGE_MAX_RECORDS = 32 };

static Account g_accounts[FAKE_STORAGE_MAX_ACCOUNTS];
static size_t g_account_count;
static StudentCardRecord g_records[FAKE_STORAGE_MAX_RECORDS];
static size_t g_record_count;
static AppResult g_next_result[FAKE_STORAGE_OPERATION_COUNT];
static size_t g_call_counts[FAKE_STORAGE_OPERATION_COUNT];

static size_t g_checks;
static size_t g_failures;

static void copy_text(char *destination, const size_t capacity,
                      const char *source) {
  const size_t length = strlen(source);
  if (capacity == 0 || length + 1 > capacity) {
    return;
  }
  memcpy(destination, source, length + 1);
}

static AppResult begin_operation(const FakeStorageOperation operation) {
  g_call_counts[operation]++;
  const AppResult result = g_next_result[operation];
  g_next_result[operation] = APP_OK;
  return result;
}

void service_test_check(const bool condition, const char *expression,
                        const int line) {
  g_checks++;
  if (!condition) {
    g_failures++;
    fprintf(stderr, "FAIL line %d: %s\n", line, expression);
  }
}

void service_test_check_result(const AppResult actual, const AppResult expected,
                               const char *expression, const int line) {
  g_checks++;
  if (actual != expected) {
    g_failures++;
    fprintf(stderr, "FAIL line %d: %s (actual=%d, expected=%d)\n", line,
            expression, actual, expected);
  }
}

int service_test_finish(const char *test_name) {
  if (g_failures == 0) {
    printf("%s: %zu checks passed\n", test_name, g_checks);
    return 0;
  }
  fprintf(stderr, "%s: %zu of %zu checks failed\n", test_name, g_failures,
          g_checks);
  return 1;
}

void fake_storage_reset() {
  memset(g_accounts, 0, sizeof g_accounts);
  memset(g_records, 0, sizeof g_records);
  memset(g_next_result, 0, sizeof g_next_result);
  memset(g_call_counts, 0, sizeof g_call_counts);
  g_account_count = 0;
  g_record_count = 0;
}

void fake_storage_fail_next(const FakeStorageOperation operation,
                            const AppResult result) {
  g_next_result[operation] = result;
}

size_t fake_storage_call_count(const FakeStorageOperation operation) {
  return g_call_counts[operation];
}

AppResult fake_storage_seed_account(const Account *account) {
  if (account == nullptr || g_account_count >= FAKE_STORAGE_MAX_ACCOUNTS) {
    return APP_ERR_NO_MEMORY;
  }
  if (fake_storage_find_account(account->username) != nullptr) {
    return APP_ERR_ALREADY_EXISTS;
  }
  g_accounts[g_account_count++] = *account;
  return APP_OK;
}

AppResult fake_storage_seed_account_with_password(const char *username,
                                                  const char *password,
                                                  const UserRole role,
                                                  const char *student_id) {
  if (username == nullptr || password == nullptr || student_id == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  Account account = {0};
  copy_text(account.username, sizeof account.username, username);
  copy_text(account.student_id, sizeof account.student_id, student_id);
  account.role = role;
  const AppResult result = password_create(password, account.password_salt_hex,
                                           account.password_hash_hex);
  if (result != APP_OK) {
    return result;
  }
  const AppResult seed_result = fake_storage_seed_account(&account);
  memset(&account, 0, sizeof account);
  return seed_result;
}

AppResult fake_storage_seed_record(const StudentCardRecord *record) {
  if (record == nullptr || g_record_count >= FAKE_STORAGE_MAX_RECORDS) {
    return APP_ERR_NO_MEMORY;
  }
  if (fake_storage_find_record(record->student_id) != nullptr) {
    return APP_ERR_ALREADY_EXISTS;
  }
  g_records[g_record_count++] = *record;
  return APP_OK;
}

const Account *fake_storage_find_account(const char *username) {
  if (username == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < g_account_count; i++) {
    if (strcmp(g_accounts[i].username, username) == 0) {
      return &g_accounts[i];
    }
  }
  return nullptr;
}

const StudentCardRecord *fake_storage_find_record(const char *student_id) {
  if (student_id == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < g_record_count; i++) {
    if (strcmp(g_records[i].student_id, student_id) == 0) {
      return &g_records[i];
    }
  }
  return nullptr;
}

size_t fake_storage_account_count_seeded() { return g_account_count; }

size_t fake_storage_record_count_seeded() { return g_record_count; }

AppResult storage_init(const char *data_dir) {
  return data_dir == nullptr ? APP_ERR_INVALID_ARGUMENT : APP_OK;
}

AppResult storage_account_load(const char *username, Account *out) {
  const AppResult failure = begin_operation(FAKE_STORAGE_ACCOUNT_LOAD);
  if (failure != APP_OK) {
    return failure;
  }
  if (username == nullptr || out == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  const Account *account = fake_storage_find_account(username);
  if (account == nullptr) {
    return APP_ERR_NOT_FOUND;
  }
  *out = *account;
  return APP_OK;
}

AppResult storage_account_save(const Account *account) {
  const AppResult failure = begin_operation(FAKE_STORAGE_ACCOUNT_SAVE);
  if (failure != APP_OK) {
    return failure;
  }
  if (account == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  for (size_t i = 0; i < g_account_count; i++) {
    if (strcmp(g_accounts[i].username, account->username) == 0) {
      g_accounts[i] = *account;
      return APP_OK;
    }
  }
  if (g_account_count >= FAKE_STORAGE_MAX_ACCOUNTS) {
    return APP_ERR_NO_MEMORY;
  }
  g_accounts[g_account_count++] = *account;
  return APP_OK;
}

AppResult storage_account_delete(const char *username) {
  const AppResult failure = begin_operation(FAKE_STORAGE_ACCOUNT_DELETE);
  if (failure != APP_OK) {
    return failure;
  }
  if (username == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  for (size_t i = 0; i < g_account_count; i++) {
    if (strcmp(g_accounts[i].username, username) == 0) {
      memmove(&g_accounts[i], &g_accounts[i + 1],
              (g_account_count - i - 1) * sizeof g_accounts[0]);
      g_account_count--;
      memset(&g_accounts[g_account_count], 0, sizeof g_accounts[0]);
      return APP_OK;
    }
  }
  return APP_ERR_NOT_FOUND;
}

AppResult storage_account_list(AccountList *out) {
  const AppResult failure = begin_operation(FAKE_STORAGE_ACCOUNT_LIST);
  if (failure != APP_OK) {
    return failure;
  }
  if (out == nullptr || out->size != 0) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  for (size_t i = 0; i < g_account_count; i++) {
    if (!account_list_append(out, &g_accounts[i])) {
      account_list_clear(out);
      return APP_ERR_NO_MEMORY;
    }
  }
  return APP_OK;
}

AppResult storage_record_load(const char *student_id, StudentCardRecord *out) {
  const AppResult failure = begin_operation(FAKE_STORAGE_RECORD_LOAD);
  if (failure != APP_OK) {
    return failure;
  }
  if (student_id == nullptr || out == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  const StudentCardRecord *record = fake_storage_find_record(student_id);
  if (record == nullptr) {
    return APP_ERR_NOT_FOUND;
  }
  *out = *record;
  return APP_OK;
}

AppResult storage_record_save(const StudentCardRecord *record) {
  const AppResult failure = begin_operation(FAKE_STORAGE_RECORD_SAVE);
  if (failure != APP_OK) {
    return failure;
  }
  if (record == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  for (size_t i = 0; i < g_record_count; i++) {
    if (strcmp(g_records[i].student_id, record->student_id) == 0) {
      g_records[i] = *record;
      return APP_OK;
    }
  }
  if (g_record_count >= FAKE_STORAGE_MAX_RECORDS) {
    return APP_ERR_NO_MEMORY;
  }
  g_records[g_record_count++] = *record;
  return APP_OK;
}

AppResult storage_record_list(StudentList *out) {
  const AppResult failure = begin_operation(FAKE_STORAGE_RECORD_LIST);
  if (failure != APP_OK) {
    return failure;
  }
  if (out == nullptr || out->size != 0) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  for (size_t i = 0; i < g_record_count; i++) {
    if (!student_list_append(out, &g_records[i])) {
      student_list_clear(out);
      return APP_ERR_NO_MEMORY;
    }
  }
  return APP_OK;
}

AppResult storage_record_find_by_name(const char *name, StudentList *out) {
  const AppResult failure = begin_operation(FAKE_STORAGE_RECORD_FIND_BY_NAME);
  if (failure != APP_OK) {
    return failure;
  }
  if (name == nullptr || out == nullptr || out->size != 0) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  for (size_t i = 0; i < g_record_count; i++) {
    if (strcmp(g_records[i].name, name) == 0 &&
        !student_list_append(out, &g_records[i])) {
      student_list_clear(out);
      return APP_ERR_NO_MEMORY;
    }
  }
  return APP_OK;
}

AppResult storage_name_index_append(const char *name, const char *student_id) {
  const AppResult failure = begin_operation(FAKE_STORAGE_NAME_INDEX_APPEND);
  if (failure != APP_OK) {
    return failure;
  }
  if (name == nullptr || student_id == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  return APP_OK;
}

AppResult storage_account_count(size_t *out_count) {
  const AppResult failure = begin_operation(FAKE_STORAGE_ACCOUNT_COUNT);
  if (failure != APP_OK) {
    return failure;
  }
  if (out_count == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  *out_count = g_account_count;
  return APP_OK;
}

AppResult storage_shutdown() { return APP_OK; }
