#include "service_test_support.h"
#include "services/auth_service.h"

#include <string.h>

#define CHECK(condition) service_test_check((condition), #condition, __LINE__)

static void test_bootstrap_login_and_change_password() {
  fake_storage_reset();

  bool needed = false;
  CHECK(auth_needs_bootstrap(nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(auth_needs_bootstrap(&needed) == APP_OK);
  CHECK(needed);

  CHECK(auth_bootstrap_admin(nullptr, "adminpass") == APP_ERR_INVALID_ARGUMENT);
  CHECK(auth_bootstrap_admin("admin", nullptr) == APP_ERR_INVALID_ARGUMENT);
  CHECK(auth_bootstrap_admin("bad name", "adminpass") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(auth_bootstrap_admin("admin", "short") == APP_ERR_INVALID_ARGUMENT);
  CHECK(auth_bootstrap_admin("admin", "adminpass") == APP_OK);
  CHECK(fake_storage_account_count_seeded() == 1);

  needed = true;
  CHECK(auth_needs_bootstrap(&needed) == APP_OK);
  CHECK(!needed);
  CHECK(auth_bootstrap_admin("second", "adminpass") == APP_ERR_ALREADY_EXISTS);

  Account session = {0};
  CHECK(auth_login(nullptr, "adminpass", &session) == APP_ERR_INVALID_ARGUMENT);
  CHECK(auth_login("admin", nullptr, &session) == APP_ERR_INVALID_ARGUMENT);
  CHECK(auth_login("bad name", "adminpass", &session) == APP_ERR_AUTH_FAILED);
  CHECK(auth_login("admin", "short", &session) == APP_ERR_AUTH_FAILED);
  CHECK(auth_login("missing", "adminpass", &session) == APP_ERR_AUTH_FAILED);
  CHECK(auth_login("admin", "wrongpass", &session) == APP_ERR_AUTH_FAILED);
  CHECK(session.username[0] == '\0');

  CHECK(auth_login("admin", "adminpass", &session) == APP_OK);
  CHECK(strcmp(session.username, "admin") == 0);
  CHECK(session.role == ROLE_ADMIN);
  CHECK(session.student_id[0] == '\0');
  CHECK(session.password_salt_hex[0] != '\0');
  CHECK(session.password_hash_hex[0] != '\0');

  CHECK(auth_change_password(nullptr, "adminpass", "newpass66") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(auth_change_password(&session, nullptr, "newpass66") ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(auth_change_password(&session, "adminpass", nullptr) ==
        APP_ERR_INVALID_ARGUMENT);
  CHECK(auth_change_password(&session, "short", "newpass66") ==
        APP_ERR_AUTH_FAILED);
  CHECK(auth_change_password(&session, "adminpass", "short") ==
        APP_ERR_INVALID_ARGUMENT);
  Account malformed_actor = session;
  malformed_actor.password_hash_hex[0] = 'g';
  CHECK(auth_change_password(&malformed_actor, "adminpass", "newpass66") ==
        APP_ERR_INVALID_DATA);
  CHECK(auth_change_password(&session, "wrongpass", "newpass66") ==
        APP_ERR_AUTH_FAILED);
  CHECK(auth_change_password(&session, "adminpass", "newpass66") == APP_OK);

  Account changed = {0};
  CHECK(auth_login("admin", "adminpass", &changed) == APP_ERR_AUTH_FAILED);
  CHECK(auth_login("admin", "newpass66", &changed) == APP_OK);
  CHECK(strcmp(changed.username, "admin") == 0);
  CHECK(auth_change_password(&session, "newpass66", "nextpass") == APP_OK);
  CHECK(auth_login("admin", "newpass66", &changed) == APP_ERR_AUTH_FAILED);
  CHECK(auth_login("admin", "nextpass", &changed) == APP_OK);

  memset(&session, 0, sizeof session);
  memset(&changed, 0, sizeof changed);
  memset(&malformed_actor, 0, sizeof malformed_actor);
}

static void test_authentication_storage_and_credential_errors() {
  fake_storage_reset();

  bool needed = false;
  fake_storage_fail_next(FAKE_STORAGE_ACCOUNT_COUNT, APP_ERR_IO);
  CHECK(auth_needs_bootstrap(&needed) == APP_ERR_IO);
  CHECK(!needed);

  fake_storage_reset();
  fake_storage_fail_next(FAKE_STORAGE_ACCOUNT_COUNT, APP_ERR_INVALID_DATA);
  CHECK(auth_bootstrap_admin("admin", "adminpass") == APP_ERR_INVALID_DATA);
  CHECK(fake_storage_call_count(FAKE_STORAGE_ACCOUNT_SAVE) == 0);

  fake_storage_reset();
  fake_storage_fail_next(FAKE_STORAGE_ACCOUNT_SAVE, APP_ERR_IO);
  CHECK(auth_bootstrap_admin("admin", "adminpass") == APP_ERR_IO);
  CHECK(fake_storage_account_count_seeded() == 0);

  fake_storage_reset();
  Account account = {0};
  memcpy(account.username, "admin", sizeof "admin");
  account.role = ROLE_ADMIN;
  CHECK(fake_storage_seed_account(&account) == APP_OK);
  fake_storage_fail_next(FAKE_STORAGE_ACCOUNT_LOAD, APP_ERR_IO);
  CHECK(auth_login("admin", "adminpass", &account) == APP_ERR_IO);

  fake_storage_reset();
  CHECK(fake_storage_seed_account_with_password("admin", "adminpass",
                                                ROLE_ADMIN, "") == APP_OK);
  Account session = {0};
  CHECK(auth_login("admin", "adminpass", &session) == APP_OK);
  fake_storage_fail_next(FAKE_STORAGE_ACCOUNT_SAVE, APP_ERR_IO);
  CHECK(auth_change_password(&session, "adminpass", "newpass66") == APP_ERR_IO);

  Account unchanged = {0};
  CHECK(auth_login("admin", "adminpass", &unchanged) == APP_OK);
  memset(&session, 0, sizeof session);
  memset(&unchanged, 0, sizeof unchanged);
  memset(&account, 0, sizeof account);
}

int main() {
  test_bootstrap_login_and_change_password();
  test_authentication_storage_and_credential_errors();
  return service_test_finish("test_auth_service");
}
