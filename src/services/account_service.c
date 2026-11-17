#include "services/account_service.h"

#include "password.h"
#include "service_helpers.h"
#include "storage.h"

#include <string.h>

static AppResult check_admin_actor(const Account *actor) {
  if (actor == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_account_is_valid(actor)) {
    return APP_ERR_INVALID_DATA;
  }
  if (actor->role != ROLE_ADMIN) {
    return APP_ERR_PERMISSION_DENIED;
  }
  return APP_OK;
}

static bool role_is_valid(const UserRole role) {
  return role == ROLE_ADMIN || role == ROLE_CARD_USER ||
         role == ROLE_CARD_CLERK;
}

AppResult account_create(const Account *actor, const char *username,
                         const char *password, const UserRole role,
                         const char *student_id) {
  AppResult result = check_admin_actor(actor);
  if (result != APP_OK) {
    return result;
  }
  if (username == nullptr || password == nullptr || student_id == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_username_is_valid(username) ||
      !service_password_is_valid(password) || !role_is_valid(role)) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (role == ROLE_CARD_USER) {
    if (!service_student_id_is_valid(student_id)) {
      return APP_ERR_INVALID_ARGUMENT;
    }
  } else if (student_id[0] != '\0') {
    return APP_ERR_INVALID_ARGUMENT;
  }

  Account existing = {0};
  result = storage_account_load(username, &existing);
  if (result == APP_OK) {
    if (!service_account_is_valid(&existing) ||
        strcmp(existing.username, username) != 0) {
      return APP_ERR_INVALID_DATA;
    }
    return APP_ERR_ALREADY_EXISTS;
  }
  if (result != APP_ERR_NOT_FOUND) {
    return result;
  }

  if (role == ROLE_CARD_USER) {
    StudentCardRecord record = {0};
    result = storage_record_load(student_id, &record);
    if (result != APP_OK) {
      return result;
    }
    if (!service_student_record_is_valid(&record) ||
        strcmp(record.student_id, student_id) != 0) {
      return APP_ERR_INVALID_DATA;
    }
  }

  Account account = {0};
  service_copy_text(account.username, sizeof account.username, username);
  service_copy_text(account.student_id, sizeof account.student_id, student_id);
  account.role = role;
  result = password_create(password, account.password_salt_hex,
                           account.password_hash_hex);
  if (result != APP_OK) {
    return result;
  }
  return storage_account_save(&account);
}

AppResult account_get(const Account *actor, const char *username,
                      Account *out) {
  AppResult result = check_admin_actor(actor);
  if (result != APP_OK) {
    return result;
  }
  if (username == nullptr || out == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_username_is_valid(username)) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  Account account = {0};
  result = storage_account_load(username, &account);
  if (result != APP_OK) {
    return result;
  }
  if (!service_account_is_valid(&account) ||
      strcmp(account.username, username) != 0) {
    return APP_ERR_INVALID_DATA;
  }
  *out = account;
  return APP_OK;
}

AppResult account_list(const Account *actor, AccountList *out) {
  AppResult result = check_admin_actor(actor);
  if (result != APP_OK) {
    return result;
  }
  if (out == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (out->size != 0) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  result = storage_account_list(out);
  if (result != APP_OK) {
    account_list_clear(out);
    return result;
  }
  for (const AccountNode *node = out->head; node != nullptr;
       node = node->next) {
    if (!service_account_is_valid(&node->value)) {
      account_list_clear(out);
      return APP_ERR_INVALID_DATA;
    }
  }
  return APP_OK;
}

AppResult account_delete(const Account *actor, const char *username) {
  const AppResult result = check_admin_actor(actor);
  if (result != APP_OK) {
    return result;
  }
  if (username == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_username_is_valid(username)) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (strcmp(actor->username, username) == 0) {
    return APP_ERR_PERMISSION_DENIED;
  }

  return storage_account_delete(username);
}
