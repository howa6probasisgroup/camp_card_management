#ifndef CAMP_CARD_SERVICES_SERVICE_HELPERS_H
#define CAMP_CARD_SERVICES_SERVICE_HELPERS_H

#include "models.h"

#include <stddef.h>

bool service_username_is_valid(const char *username);
bool service_password_is_valid(const char *password);
bool service_student_id_is_valid(const char *student_id);
bool service_name_is_valid(const char *name);
bool service_phone_is_valid(const char *phone);
bool service_profile_is_valid(const StudentProfileInput *profile);
bool service_account_is_valid(const Account *account);
bool service_student_record_is_valid(const StudentCardRecord *record);
void service_copy_text(char *destination, size_t capacity, const char *source);

#endif // CAMP_CARD_SERVICES_SERVICE_HELPERS_H
