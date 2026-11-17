#include "services/card_service.h"

#include "service_helpers.h"
#include "storage.h"

#include <stdint.h>
#include <string.h>

static AppResult check_actor(const Account *actor, const UserRole role) {
  if (actor == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_account_is_valid(actor)) {
    return APP_ERR_INVALID_DATA;
  }
  if (actor->role != role) {
    return APP_ERR_PERMISSION_DENIED;
  }
  return APP_OK;
}

static StudentCardRecord record_from_profile(const StudentProfileInput *input) {
  StudentCardRecord record = {0};
  service_copy_text(record.student_id, sizeof record.student_id,
                    input->student_id);
  service_copy_text(record.name, sizeof record.name, input->name);
  service_copy_text(record.sex, sizeof record.sex, input->sex);
  service_copy_text(record.department, sizeof record.department,
                    input->department);
  service_copy_text(record.class_name, sizeof record.class_name,
                    input->class_name);
  service_copy_text(record.phone, sizeof record.phone, input->phone);
  record.balance_cents = 0;
  record.status = CARD_STATUS_ACTIVE;
  return record;
}

static AppResult load_card_user_record(const Account *actor,
                                       StudentCardRecord *record) {
  if (!service_student_id_is_valid(actor->student_id)) {
    return APP_ERR_INVALID_DATA;
  }
  const AppResult result = storage_record_load(actor->student_id, record);
  if (result != APP_OK) {
    return result;
  }
  if (!service_student_record_is_valid(record) ||
      strcmp(record->student_id, actor->student_id) != 0) {
    return APP_ERR_INVALID_DATA;
  }
  return APP_OK;
}

AppResult card_issue(const Account *actor, const StudentProfileInput *input) {
  AppResult result = check_actor(actor, ROLE_CARD_CLERK);
  if (result != APP_OK) {
    return result;
  }
  if (input == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_profile_is_valid(input)) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  StudentCardRecord existing = {0};
  result = storage_record_load(input->student_id, &existing);
  if (result == APP_OK) {
    if (!service_student_record_is_valid(&existing) ||
        strcmp(existing.student_id, input->student_id) != 0) {
      return APP_ERR_INVALID_DATA;
    }
    return APP_ERR_ALREADY_EXISTS;
  }
  if (result != APP_ERR_NOT_FOUND) {
    return result;
  }

  const StudentCardRecord record = record_from_profile(input);
  result = storage_record_save(&record);
  if (result != APP_OK) {
    return result;
  }
  return storage_name_index_append(record.name, record.student_id);
}

AppResult card_get_by_id(const Account *actor, const char *student_id,
                         StudentCardRecord *out) {
  if (actor == nullptr || student_id == nullptr || out == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_account_is_valid(actor)) {
    return APP_ERR_INVALID_DATA;
  }
  if (actor->role != ROLE_CARD_USER && actor->role != ROLE_CARD_CLERK) {
    return APP_ERR_PERMISSION_DENIED;
  }
  if (actor->role == ROLE_CARD_USER &&
      !service_student_id_is_valid(actor->student_id)) {
    return APP_ERR_INVALID_DATA;
  }
  if (!service_student_id_is_valid(student_id)) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (actor->role == ROLE_CARD_USER &&
      strcmp(actor->student_id, student_id) != 0) {
    return APP_ERR_PERMISSION_DENIED;
  }

  StudentCardRecord record = {0};
  const AppResult result = storage_record_load(student_id, &record);
  if (result != APP_OK) {
    return result;
  }
  if (!service_student_record_is_valid(&record) ||
      strcmp(record.student_id, student_id) != 0) {
    return APP_ERR_INVALID_DATA;
  }
  *out = record;
  return APP_OK;
}

AppResult card_find_by_name(const Account *actor, const char *name,
                            StudentList *out) {
  AppResult result = check_actor(actor, ROLE_CARD_CLERK);
  if (result != APP_OK) {
    return result;
  }
  if (name == nullptr || out == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (out->size != 0) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_name_is_valid(name)) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  result = storage_record_find_by_name(name, out);
  if (result != APP_OK) {
    student_list_clear(out);
    return result;
  }
  for (const StudentNode *node = out->head; node != nullptr;
       node = node->next) {
    if (!service_student_record_is_valid(&node->value) ||
        strcmp(node->value.name, name) != 0) {
      student_list_clear(out);
      return APP_ERR_INVALID_DATA;
    }
  }
  return APP_OK;
}

AppResult card_update_own_phone(const Account *actor, const char *phone) {
  AppResult result = check_actor(actor, ROLE_CARD_USER);
  if (result != APP_OK) {
    return result;
  }
  if (phone == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_phone_is_valid(phone)) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  StudentCardRecord record = {0};
  result = load_card_user_record(actor, &record);
  if (result != APP_OK) {
    return result;
  }
  service_copy_text(record.phone, sizeof record.phone, phone);
  return storage_record_save(&record);
}

AppResult card_recharge(const Account *actor, const int64_t amount_cents,
                        int64_t *new_balance_cents) {
  AppResult result = check_actor(actor, ROLE_CARD_USER);
  if (result != APP_OK) {
    return result;
  }
  if (new_balance_cents == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (amount_cents <= 0) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  StudentCardRecord record = {0};
  result = load_card_user_record(actor, &record);
  if (result != APP_OK) {
    return result;
  }
  if (record.status == CARD_STATUS_FROZEN) {
    return APP_ERR_CARD_FROZEN;
  }
  if (amount_cents > INT64_MAX - record.balance_cents) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  record.balance_cents += amount_cents;
  result = storage_record_save(&record);
  if (result == APP_OK) {
    *new_balance_cents = record.balance_cents;
  }
  return result;
}

AppResult card_consume(const Account *actor, const int64_t amount_cents,
                       int64_t *new_balance_cents) {
  AppResult result = check_actor(actor, ROLE_CARD_USER);
  if (result != APP_OK) {
    return result;
  }
  if (new_balance_cents == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (amount_cents <= 0) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  StudentCardRecord record = {0};
  result = load_card_user_record(actor, &record);
  if (result != APP_OK) {
    return result;
  }
  if (record.status == CARD_STATUS_FROZEN) {
    return APP_ERR_CARD_FROZEN;
  }
  if (amount_cents > record.balance_cents) {
    return APP_ERR_INSUFFICIENT_BALANCE;
  }

  record.balance_cents -= amount_cents;
  result = storage_record_save(&record);
  if (result == APP_OK) {
    *new_balance_cents = record.balance_cents;
  }
  return result;
}

AppResult card_set_status(const Account *actor, const char *student_id,
                          const CardStatus status) {
  AppResult result = check_actor(actor, ROLE_CARD_CLERK);
  if (result != APP_OK) {
    return result;
  }
  if (student_id == nullptr) {
    return APP_ERR_INVALID_ARGUMENT;
  }
  if (!service_student_id_is_valid(student_id) ||
      (status != CARD_STATUS_ACTIVE && status != CARD_STATUS_FROZEN)) {
    return APP_ERR_INVALID_ARGUMENT;
  }

  StudentCardRecord record = {0};
  result = storage_record_load(student_id, &record);
  if (result != APP_OK) {
    return result;
  }
  if (!service_student_record_is_valid(&record) ||
      strcmp(record.student_id, student_id) != 0) {
    return APP_ERR_INVALID_DATA;
  }
  if (record.status == status) {
    return APP_OK;
  }

  record.status = status;
  return storage_record_save(&record);
}
