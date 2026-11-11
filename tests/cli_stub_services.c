#include "services/account_service.h"
#include "services/auth_service.h"
#include "services/card_service.h"

// CLI 测试使用的业务服务桩。仅用于验证菜单输入与调用映射，不实现任何
// 业务规则，也不读写存储。成功路径返回空结果，失败路径返回约定的错误码。

AppResult auth_needs_bootstrap(bool *out_needed) {
  if (out_needed == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  *out_needed = false;
  return APP_OK;
}

AppResult auth_bootstrap_admin(const char *username, const char *password) {
  (void)username;
  (void)password;
  return APP_ERR_ALREADY_EXISTS;
}

AppResult auth_login(const char *username, const char *password, Account *) {
  (void)username;
  (void)password;
  return APP_ERR_AUTH_FAILED;
}

AppResult auth_change_password(const Account *actor, const char *old_password,
                               const char *new_password) {
  (void)actor;
  (void)old_password;
  (void)new_password;
  return APP_OK;
}

AppResult account_create(const Account *actor, const char *username,
                         const char *password, UserRole,
                         const char *student_id) {
  (void)actor;
  (void)username;
  (void)password;
  (void)student_id;
  return APP_OK;
}

AppResult account_get(const Account *actor, const char *username, Account *) {
  (void)actor;
  (void)username;
  return APP_ERR_NOT_FOUND;
}

AppResult account_list(const Account *actor, AccountList *) {
  (void)actor;
  return APP_OK;
}

AppResult account_delete(const Account *actor, const char *username) {
  (void)actor;
  (void)username;
  return APP_OK;
}

AppResult card_issue(const Account *actor, const StudentProfileInput *input) {
  (void)actor;
  (void)input;
  return APP_OK;
}

AppResult card_get_by_id(const Account *actor, const char *student_id,
                         StudentCardRecord *) {
  (void)actor;
  (void)student_id;
  return APP_ERR_NOT_FOUND;
}

AppResult card_find_by_name(const Account *actor, const char *name,
                            StudentList *) {
  (void)actor;
  (void)name;
  return APP_OK;
}

AppResult card_update_own_phone(const Account *actor, const char *phone) {
  (void)actor;
  (void)phone;
  return APP_OK;
}

AppResult card_recharge(const Account *actor, int64_t, int64_t *) {
  (void)actor;
  return APP_OK;
}

AppResult card_consume(const Account *actor, int64_t, int64_t *) {
  (void)actor;
  return APP_OK;
}

AppResult card_set_status(const Account *actor, const char *student_id,
                          CardStatus) {
  (void)actor;
  (void)student_id;
  return APP_OK;
}
