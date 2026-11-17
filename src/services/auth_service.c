#include "services/auth_service.h"

#include "password.h"
#include "service_helpers.h"
#include "storage.h"

#include <string.h>

AppResult auth_needs_bootstrap(bool *out_needed) {
  if (out_needed == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  size_t account_count = 0;
  const AppResult result = storage_account_count(&account_count);
  if (result != APP_OK) {
    return result;
  }
  *out_needed = account_count == 0;
  return APP_OK;
}

AppResult auth_login(const char *username, const char *password, Account *out) {
  if (username == nullptr || password == nullptr || out == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_username_is_valid(username) ||
      !service_password_is_valid(password)) {
    return APP_ERR_AUTH_FAILED;
  }

  Account account = {0};
  AppResult result = storage_account_load(username, &account);
  if (result == APP_ERR_NOT_FOUND) {
    return APP_ERR_AUTH_FAILED;
  }
  if (result != APP_OK) {
    return result;
  }
  if (strcmp(account.username, username) != 0) {
    return APP_ERR_INVALID_DATA;
  }

  result = password_verify(password, account.password_salt_hex,
                           account.password_hash_hex);
  if (result != APP_OK) {
    return result;
  }
  if (!service_account_is_valid(&account)) {
    return APP_ERR_INVALID_DATA;
  }

  *out = account;
  return APP_OK;
}

AppResult auth_bootstrap_admin(const char *username, const char *password) {
  if (username == nullptr || password == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_username_is_valid(username) ||
      !service_password_is_valid(password)) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  size_t account_count = 0;
  AppResult result = storage_account_count(&account_count);
  if (result != APP_OK) {
    return result;
  }
  if (account_count != 0) {
    return APP_ERR_ALREADY_EXISTS;
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

  Account account = {0};
  service_copy_text(account.username, sizeof account.username, username);
  account.role = ROLE_ADMIN;
  result = password_create(password, account.password_salt_hex,
                           account.password_hash_hex);
  if (result != APP_OK) {
    return result;
  }
  return storage_account_save(&account);
}

AppResult auth_change_password(const Account *actor, const char *old_password,
                               const char *new_password) {
  if (actor == nullptr || old_password == nullptr || new_password == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_account_is_valid(actor)) {
    return APP_ERR_INVALID_DATA;
  }
  if (!service_password_is_valid(old_password)) {
    return APP_ERR_AUTH_FAILED;
  }
  if (!service_password_is_valid(new_password)) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  Account account = {0};
  AppResult result = storage_account_load(actor->username, &account);
  if (result != APP_OK) {
    return result;
  }
  if (!service_account_is_valid(&account)) {
    return APP_ERR_INVALID_DATA;
  }

  result = password_verify(old_password, account.password_salt_hex,
                           account.password_hash_hex);
  if (result != APP_OK) {
    return result;
  }

  char salt[PASSWORD_SALT_HEX_LENGTH + 1] = {0};
  char hash[PASSWORD_HASH_HEX_LENGTH + 1] = {0};
  result = password_create(new_password, salt, hash);
  if (result != APP_OK) {
    return result;
  }

  service_copy_text(account.password_salt_hex, sizeof account.password_salt_hex,
                    salt);
  service_copy_text(account.password_hash_hex, sizeof account.password_hash_hex,
                    hash);
  return storage_account_save(&account);
}
