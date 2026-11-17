#include "service_helpers.h"

#include <string.h>

static bool is_ascii_digit(const char value) {
  return value >= '0' && value <= '9';
}

static bool is_ascii_letter(const char value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static bool is_lower_hex(const char value) {
  return is_ascii_digit(value) || (value >= 'a' && value <= 'f');
}

static size_t bounded_length(const char *text, const size_t maximum) {
  size_t length = 0;
  while (length <= maximum && text[length] != '\0') {
    length++;
  }
  return length;
}

static bool exact_length(const char *text, const size_t expected) {
  return bounded_length(text, expected) == expected;
}

static bool lower_hex_has_length(const char *text, const size_t expected) {
  if (!exact_length(text, expected)) {
    return false;
  }
  for (size_t i = 0; i < expected; i++) {
    if (!is_lower_hex(text[i])) {
      return false;
    }
  }
  return true;
}

static bool utf8_text_is_valid(const char *text, const size_t maximum,
                               const bool allow_empty) {
  const size_t length = bounded_length(text, maximum);
  if (length > maximum || (!allow_empty && length == 0)) {
    return false;
  }

  size_t index = 0;
  while (index < length) {
    const unsigned char first = (unsigned char)text[index];
    if (first <= 0x7f) {
      index++;
      continue;
    }

    size_t continuation_count;
    unsigned char second_minimum = 0x80;
    unsigned char second_maximum = 0xbf;
    if (first >= 0xc2 && first <= 0xdf) {
      continuation_count = 1;
    } else if (first == 0xe0) {
      continuation_count = 2;
      second_minimum = 0xa0;
    } else if ((first >= 0xe1 && first <= 0xec) ||
               (first >= 0xee && first <= 0xef)) {
      continuation_count = 2;
    } else if (first == 0xed) {
      continuation_count = 2;
      second_maximum = 0x9f;
    } else if (first == 0xf0) {
      continuation_count = 3;
      second_minimum = 0x90;
    } else if (first >= 0xf1 && first <= 0xf3) {
      continuation_count = 3;
    } else if (first == 0xf4) {
      continuation_count = 3;
      second_maximum = 0x8f;
    } else {
      return false;
    }

    if (length - index <= continuation_count) {
      return false;
    }
    const unsigned char second = (unsigned char)text[index + 1];
    if (second < second_minimum || second > second_maximum) {
      return false;
    }
    for (size_t offset = 2; offset <= continuation_count; offset++) {
      const unsigned char continuation = (unsigned char)text[index + offset];
      if (continuation < 0x80 || continuation > 0xbf) {
        return false;
      }
    }
    index += continuation_count + 1;
  }
  return true;
}

static bool text_equals(const char *text, const char *expected) {
  const size_t expected_length = strlen(expected);
  return exact_length(text, expected_length) &&
         memcmp(text, expected, expected_length) == 0;
}

static bool profile_fields_are_valid(const char *student_id, const char *name,
                                     const char *sex, const char *department,
                                     const char *class_name,
                                     const char *phone) {
  if (!service_student_id_is_valid(student_id) ||
      !utf8_text_is_valid(name, NAME_MAX_BYTES, false) ||
      (!text_equals(sex, "男") && !text_equals(sex, "女") &&
       !text_equals(sex, "其他"))) {
    return false;
  }
  return utf8_text_is_valid(department, DEPARTMENT_MAX_BYTES, false) &&
         utf8_text_is_valid(class_name, CLASS_NAME_MAX_BYTES, false) &&
         service_phone_is_valid(phone);
}

bool service_username_is_valid(const char *username) {
  if (username == nullptr) {
    return false;
  }
  const size_t length = bounded_length(username, USERNAME_MAX_BYTES);
  if (length == 0 || length > USERNAME_MAX_BYTES) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    const char value = username[i];
    if (!is_ascii_letter(value) && !is_ascii_digit(value) && value != '_' &&
        value != '-' && value != '.') {
      return false;
    }
  }
  return true;
}

bool service_password_is_valid(const char *password) {
  if (password == nullptr) {
    return false;
  }
  const size_t length = bounded_length(password, PASSWORD_MAX_BYTES);
  return length >= PASSWORD_MIN_BYTES && length <= PASSWORD_MAX_BYTES;
}

bool service_student_id_is_valid(const char *student_id) {
  if (student_id == nullptr || !exact_length(student_id, STUDENT_ID_LENGTH)) {
    return false;
  }
  for (size_t i = 0; i < STUDENT_ID_LENGTH; i++) {
    if (!is_ascii_digit(student_id[i])) {
      return false;
    }
  }
  return true;
}

bool service_name_is_valid(const char *name) {
  return utf8_text_is_valid(name, NAME_MAX_BYTES, false);
}

bool service_phone_is_valid(const char *phone) {
  if (phone == nullptr ||
      bounded_length(phone, PHONE_MAX_BYTES) > PHONE_MAX_BYTES) {
    return false;
  }
  for (const char *cursor = phone; *cursor != '\0'; cursor++) {
    if (!is_ascii_digit(*cursor) && *cursor != '+' && *cursor != '-' &&
        *cursor != ' ') {
      return false;
    }
  }
  return true;
}

bool service_profile_is_valid(const StudentProfileInput *profile) {
  return profile != nullptr &&
         profile_fields_are_valid(profile->student_id, profile->name,
                                  profile->sex, profile->department,
                                  profile->class_name, profile->phone);
}

bool service_account_is_valid(const Account *account) {
  if (account == nullptr || !service_username_is_valid(account->username) ||
      !lower_hex_has_length(account->password_salt_hex,
                            PASSWORD_SALT_HEX_LENGTH) ||
      !lower_hex_has_length(account->password_hash_hex,
                            PASSWORD_HASH_HEX_LENGTH)) {
    return false;
  }

  switch (account->role) {
  case ROLE_CARD_USER:
    return service_student_id_is_valid(account->student_id);
  case ROLE_ADMIN:
  case ROLE_CARD_CLERK:
    return account->student_id[0] == '\0';
  default:
    return false;
  }
}

bool service_student_record_is_valid(const StudentCardRecord *record) {
  return record != nullptr && record->balance_cents >= 0 &&
         (record->status == CARD_STATUS_ACTIVE ||
          record->status == CARD_STATUS_FROZEN) &&
         profile_fields_are_valid(record->student_id, record->name, record->sex,
                                  record->department, record->class_name,
                                  record->phone);
}

void service_copy_text(char *destination, const size_t capacity,
                       const char *source) {
  const size_t length = strlen(source);
  if (capacity == 0 || length + 1 > capacity) {
    return;
  }
  memcpy(destination, source, length + 1);
}
