#include "storage.h"
#include "file_access.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

static char *accounts_dir, *students_dir, *names_dir;
static const char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
static const char *roles[] = {"admin", "card_user", "card_clerk"};

static size_t bounded_length(const char *s, size_t max) {
  size_t n = 0;
  if (!s)
    return max + 1;
  while (n <= max && s[n])
    n++;
  return n;
}
static bool digits(const char *s, size_t n) {
  if (bounded_length(s, n) != n)
    return false;
  for (size_t i = 0; i < n; i++)
    if (s[i] < '0' || s[i] > '9')
      return false;
  return true;
}
static bool username_valid(const char *s) {
  size_t n = bounded_length(s, USERNAME_MAX_BYTES);
  if (!n || n > USERNAME_MAX_BYTES)
    return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
      return false;
  }
  return true;
}
static bool hex_valid(const char *s, size_t n) {
  if (bounded_length(s, n) != n)
    return false;
  for (size_t i = 0; i < n; i++)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
      return false;
  return true;
}
/* 严格 UTF-8：排除过长编码、代理项及超出 Unicode 上限的码点。 */
static bool text_valid(const char *s, size_t max, bool empty) {
  size_t n = bounded_length(s, max);
  if (n > max || (!empty && !n))
    return false;
  for (size_t i = 0; i < n;) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80)
      continue;
    unsigned count, low = 0x80, high = 0xbf;
    if (c >= 0xc2 && c <= 0xdf)
      count = 1;
    else if (c >= 0xe0 && c <= 0xef) {
      count = 2;
      if (c == 0xe0)
        low = 0xa0;
      if (c == 0xed)
        high = 0x9f;
    } else if (c >= 0xf0 && c <= 0xf4) {
      count = 3;
      if (c == 0xf0)
        low = 0x90;
      if (c == 0xf4)
        high = 0x8f;
    } else
      return false;
    if (n - i < count || (unsigned char)s[i] < low ||
        (unsigned char)s[i] > high)
      return false;
    for (unsigned j = 1; j < count; j++)
      if ((unsigned char)s[i + j] < 0x80 || (unsigned char)s[i + j] > 0xbf)
        return false;
    i += count;
  }
  return true;
}
static bool account_valid(const Account *a) {
  return a && username_valid(a->username) &&
         hex_valid(a->password_salt_hex, PASSWORD_SALT_HEX_LENGTH) &&
         hex_valid(a->password_hash_hex, PASSWORD_HASH_HEX_LENGTH) &&
         a->role >= ROLE_ADMIN && a->role <= ROLE_CARD_CLERK &&
         (a->role == ROLE_CARD_USER ? digits(a->student_id, STUDENT_ID_LENGTH)
                                    : a->student_id[0] == '\0');
}
static bool record_valid(const StudentCardRecord *r) {
  if (!r || !digits(r->student_id, STUDENT_ID_LENGTH) ||
      !text_valid(r->name, NAME_MAX_BYTES, false) ||
      !text_valid(r->department, DEPARTMENT_MAX_BYTES, false) ||
      !text_valid(r->class_name, CLASS_NAME_MAX_BYTES, false) ||
      !text_valid(r->sex, SEX_MAX_BYTES, false) ||
      (strcmp(r->sex, "男") != 0 && strcmp(r->sex, "女") != 0 &&
       strcmp(r->sex, "其他") != 0) ||
      bounded_length(r->phone, PHONE_MAX_BYTES) > PHONE_MAX_BYTES ||
      r->balance_cents < 0 ||
      (r->status != CARD_STATUS_ACTIVE && r->status != CARD_STATUS_FROZEN))
    return false;
  for (const char *s = r->phone; *s; s++)
    if (!((*s >= '0' && *s <= '9') || *s == '+' || *s == '-' || *s == ' '))
      return false;
  return true;
}

static void encode(const char *text, char *out) {
  const unsigned char *s = (const unsigned char *)text;
  size_t n = strlen(text), k = 0;
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = (unsigned)s[i] << 16;
    if (i + 1 < n)
      v |= (unsigned)s[i + 1] << 8;
    if (i + 2 < n)
      v |= s[i + 2];
    out[k++] = alphabet[v >> 18];
    out[k++] = alphabet[(v >> 12) & 63];
    if (i + 1 < n)
      out[k++] = alphabet[(v >> 6) & 63];
    if (i + 2 < n)
      out[k++] = alphabet[v & 63];
  }
  out[k] = '\0';
}
static char *path_with_suffix(const char *root, const char *suffix) {
  size_t a = strlen(root), b = strlen(suffix);
  if (a > SIZE_MAX - b - 2)
    return nullptr;
  char *p = malloc(a + b + 2);
  if (p)
    snprintf(p, a + b + 2, "%s/%s", root, suffix);
  return p;
}
static char *account_path(const char *username) {
  char encoded[4 * ((USERNAME_MAX_BYTES + 2) / 3) + 6];
  encode(username, encoded);
  strcat(encoded, ".json");
  return path_with_suffix(accounts_dir, encoded);
}
static char *record_path(const char *id) {
  char suffix[32];
  snprintf(suffix, sizeof(suffix), "%.2s/%.2s/%.2s/%.1s/%.2s.json", id, id + 2,
           id + 4, id + 6, id + 7);
  return path_with_suffix(students_dir, suffix);
}
static char *name_path(const char *name) {
  char encoded[4 * ((NAME_MAX_BYTES + 2) / 3) + 1], suffix[256];
  encode(name, encoded);
  size_t k = 0;
  for (size_t i = 0; encoded[i]; i++) {
    if (i && i % 2 == 0)
      suffix[k++] = '/';
    suffix[k++] = encoded[i];
  }
  suffix[k] = '\0';
  return path_with_suffix(names_dir, suffix);
}
static AppResult ready() {
  if (!accounts_dir)
    return APP_ERR_INVALID_ARGUMENT;
  const char *dirs[] = {accounts_dir, students_dir, names_dir};
  for (size_t i = 0; i < 3; i++) {
    FileAccessKind kind;
    AppResult r = file_access_kind(dirs[i], &kind);
    if (r != APP_OK || kind != FILE_ACCESS_DIRECTORY)
      return r == APP_ERR_NO_MEMORY ? r : APP_ERR_IO;
  }
  return APP_OK;
}
AppResult storage_init(const char *data_dir) {
  if (!data_dir || !*data_dir || accounts_dir)
    return APP_ERR_INVALID_ARGUMENT;
  char *a = path_with_suffix(data_dir, "accounts");
  char *s = path_with_suffix(data_dir, "students");
  char *n = path_with_suffix(data_dir, "indexes/name");
  AppResult r = a && s && n ? APP_OK : APP_ERR_NO_MEMORY;
  if (r == APP_OK)
    r = file_access_make_directories(a);
  if (r == APP_OK)
    r = file_access_make_directories(s);
  if (r == APP_OK)
    r = file_access_make_directories(n);
  if (r != APP_OK) {
    free(a);
    free(s);
    free(n);
    return r;
  }
  accounts_dir = a;
  students_dir = s;
  names_dir = n;
  return APP_OK;
}
AppResult storage_shutdown() {
  free(accounts_dir);
  free(students_dir);
  free(names_dir);
  accounts_dir = students_dir = names_dir = nullptr;
  return APP_OK;
}

/* 对象键逐个匹配，避免只检查数量而漏过“重复键替代必需键”。 */
static bool object_fields(yyjson_val *obj, const char *const *fields,
                          size_t count) {
  if (!yyjson_is_obj(obj) || yyjson_obj_size(obj) != count)
    return false;
  unsigned seen = 0;
  yyjson_obj_iter iter = yyjson_obj_iter_with(obj);
  yyjson_val *key;
  while ((key = yyjson_obj_iter_next(&iter))) {
    size_t i;
    for (i = 0; i < count; i++)
      if (yyjson_equals_str(key, fields[i]))
        break;
    if (i == count || (seen & (1u << i)))
      return false;
    seen |= 1u << i;
  }
  return seen == (1u << count) - 1;
}
static bool get_text(yyjson_val *obj, const char *key, char *out,
                     size_t capacity) {
  yyjson_val *v = yyjson_obj_get(obj, key);
  if (!yyjson_is_str(v))
    return false;
  size_t n = yyjson_get_len(v);
  const char *s = yyjson_get_str(v);
  if (n >= capacity || memchr(s, '\0', n))
    return false;
  memcpy(out, s, n);
  out[n] = '\0';
  return true;
}
static AppResult read_document(const char *path, yyjson_doc **out) {
  char *data;
  size_t size;
  AppResult r = file_access_read(path, &data, &size);
  if (r != APP_OK)
    return r;
  yyjson_read_err error;
  yyjson_doc *doc = yyjson_read_opts(data, size, 0, nullptr, &error);
  file_access_free(data);
  if (!doc)
    return error.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION
               ? APP_ERR_NO_MEMORY
               : APP_ERR_INVALID_DATA;
  *out = doc;
  return APP_OK;
}
static AppResult account_read_path(const char *path, const char *username,
                                   Account *out) {
  yyjson_doc *doc;
  AppResult result = read_document(path, &doc);
  if (result != APP_OK)
    return result;
  yyjson_val *obj = yyjson_doc_get_root(doc);
  static const char *const fields[] = {"username", "password_salt",
                                       "password_hash", "role", "student_id"};
  Account a = {};
  char role[16];
  bool ok = object_fields(obj, fields, 5) &&
            get_text(obj, "username", a.username, sizeof(a.username)) &&
            get_text(obj, "password_salt", a.password_salt_hex,
                     sizeof(a.password_salt_hex)) &&
            get_text(obj, "password_hash", a.password_hash_hex,
                     sizeof(a.password_hash_hex)) &&
            get_text(obj, "role", role, sizeof(role)) &&
            get_text(obj, "student_id", a.student_id, sizeof(a.student_id));
  if (ok) {
    a.role = (UserRole)-1;
    for (int i = 0; i < 3; i++)
      if (!strcmp(role, roles[i]))
        a.role = (UserRole)i;
    ok = account_valid(&a) && !strcmp(a.username, username);
  }
  yyjson_doc_free(doc);
  if (!ok)
    return APP_ERR_INVALID_DATA;
  *out = a;
  return APP_OK;
}
AppResult storage_account_load(const char *username, Account *out) {
  if (!out || !username_valid(username))
    return APP_ERR_INVALID_ARGUMENT;
  AppResult r = ready();
  if (r != APP_OK)
    return r;
  char *path = account_path(username);
  if (!path)
    return APP_ERR_NO_MEMORY;
  r = account_read_path(path, username, out);
  free(path);
  return r;
}
static AppResult record_read_path(const char *path, const char *id,
                                  StudentCardRecord *out) {
  yyjson_doc *doc;
  AppResult result = read_document(path, &doc);
  if (result != APP_OK)
    return result;
  yyjson_val *obj = yyjson_doc_get_root(doc);
  static const char *const fields[] = {"student_id",    "name",       "sex",
                                       "department",    "class_name", "phone",
                                       "balance_cents", "status"};
  StudentCardRecord r = {};
  char status[16];
  bool ok = object_fields(obj, fields, 8) &&
            get_text(obj, "student_id", r.student_id, sizeof(r.student_id)) &&
            get_text(obj, "name", r.name, sizeof(r.name)) &&
            get_text(obj, "sex", r.sex, sizeof(r.sex)) &&
            get_text(obj, "department", r.department, sizeof(r.department)) &&
            get_text(obj, "class_name", r.class_name, sizeof(r.class_name)) &&
            get_text(obj, "phone", r.phone, sizeof(r.phone)) &&
            get_text(obj, "status", status, sizeof(status));
  yyjson_val *balance = yyjson_obj_get(obj, "balance_cents");
  if (ok) {
    ok = yyjson_is_int(balance) &&
         (yyjson_is_uint(balance) ? yyjson_get_uint(balance) <= INT64_MAX
                                  : yyjson_get_sint(balance) >= 0);
    if (ok) {
      r.balance_cents = yyjson_get_sint(balance);
      r.status = !strcmp(status, "active")   ? CARD_STATUS_ACTIVE
                 : !strcmp(status, "frozen") ? CARD_STATUS_FROZEN
                                             : (CardStatus)-1;
      ok = record_valid(&r) && !strcmp(r.student_id, id);
    }
  }
  yyjson_doc_free(doc);
  if (!ok)
    return APP_ERR_INVALID_DATA;
  *out = r;
  return APP_OK;
}
AppResult storage_record_load(const char *id, StudentCardRecord *out) {
  if (!out || !digits(id, STUDENT_ID_LENGTH))
    return APP_ERR_INVALID_ARGUMENT;
  AppResult r = ready();
  if (r != APP_OK)
    return r;
  char *path = record_path(id);
  if (!path)
    return APP_ERR_NO_MEMORY;
  r = record_read_path(path, id, out);
  free(path);
  return r;
}
static AppResult write_document(char *path, yyjson_mut_doc *doc,
                                bool create_parent) {
  size_t size;
  yyjson_write_err error;
  char *data =
      yyjson_mut_write_opts(doc, YYJSON_WRITE_PRETTY, nullptr, &size, &error);
  if (!data)
    return error.code == YYJSON_WRITE_ERROR_MEMORY_ALLOCATION
               ? APP_ERR_NO_MEMORY
               : APP_ERR_INVALID_ARGUMENT;
  AppResult r = APP_OK;
  if (create_parent) {
    char *last = strrchr(path, '/');
    *last = '\0';
    r = file_access_make_directories(path);
    *last = '/';
  }
  if (r == APP_OK)
    r = file_access_write(path, data, size);
  free(data);
  return r;
}
AppResult storage_account_save(const Account *a) {
  if (!account_valid(a))
    return APP_ERR_INVALID_ARGUMENT;
  AppResult r = ready();
  if (r != APP_OK)
    return r;
  char *path = account_path(a->username);
  if (!path)
    return APP_ERR_NO_MEMORY;
  /* Base64URL 大小写不同的文件名可能被文件系统视为同一路径。
   * 覆盖前必须核对实际记录的用户名，不能截断另一账号的数据。 */
  Account existing;
  r = account_read_path(path, a->username, &existing);
  if (r != APP_OK && r != APP_ERR_NOT_FOUND) {
    free(path);
    return r;
  }
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  if (!path || !doc) {
    free(path);
    yyjson_mut_doc_free(doc);
    return APP_ERR_NO_MEMORY;
  }
  yyjson_mut_val *obj = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, obj);
  bool ok =
      obj && yyjson_mut_obj_add_str(doc, obj, "username", a->username) &&
      yyjson_mut_obj_add_str(doc, obj, "password_salt", a->password_salt_hex) &&
      yyjson_mut_obj_add_str(doc, obj, "password_hash", a->password_hash_hex) &&
      yyjson_mut_obj_add_str(doc, obj, "role", roles[a->role]) &&
      yyjson_mut_obj_add_str(doc, obj, "student_id", a->student_id);
  r = ok ? write_document(path, doc, false) : APP_ERR_NO_MEMORY;
  yyjson_mut_doc_free(doc);
  free(path);
  return r;
}
AppResult storage_record_save(const StudentCardRecord *record) {
  if (!record_valid(record))
    return APP_ERR_INVALID_ARGUMENT;
  AppResult r = ready();
  if (r != APP_OK)
    return r;
  char *path = record_path(record->student_id);
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  if (!path || !doc) {
    free(path);
    yyjson_mut_doc_free(doc);
    return APP_ERR_NO_MEMORY;
  }
  yyjson_mut_val *obj = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, obj);
  bool ok =
      obj &&
      yyjson_mut_obj_add_str(doc, obj, "student_id", record->student_id) &&
      yyjson_mut_obj_add_str(doc, obj, "name", record->name) &&
      yyjson_mut_obj_add_str(doc, obj, "sex", record->sex) &&
      yyjson_mut_obj_add_str(doc, obj, "department", record->department) &&
      yyjson_mut_obj_add_str(doc, obj, "class_name", record->class_name) &&
      yyjson_mut_obj_add_str(doc, obj, "phone", record->phone) &&
      yyjson_mut_obj_add_sint(doc, obj, "balance_cents",
                              record->balance_cents) &&
      yyjson_mut_obj_add_str(doc, obj, "status",
                             record->status == CARD_STATUS_ACTIVE ? "active"
                                                                  : "frozen");
  r = ok ? write_document(path, doc, true) : APP_ERR_NO_MEMORY;
  yyjson_mut_doc_free(doc);
  free(path);
  return r;
}
AppResult storage_account_delete(const char *username) {
  if (!username_valid(username))
    return APP_ERR_INVALID_ARGUMENT;
  AppResult r = ready();
  if (r != APP_OK)
    return r;
  char *path = account_path(username);
  if (!path)
    return APP_ERR_NO_MEMORY;
  /* 删除也必须验证实际主键，防止通过文件名大小写别名删除别人的账号。 */
  Account existing;
  r = account_read_path(path, username, &existing);
  if (r == APP_OK)
    r = file_access_remove(path);
  free(path);
  return r;
}

/* 文件名必须是合法用户名的规范无填充编码，非规范条目不属于记录。 */
static bool account_filename(const char *name, char *username) {
  size_t n = strlen(name);
  if (n <= 5 || strcmp(name + n - 5, ".json") != 0)
    return false;
  n -= 5;
  if (n > 4 * ((USERNAME_MAX_BYTES + 2) / 3) || n % 4 == 1)
    return false;
  unsigned bits = 0, value = 0;
  size_t k = 0;
  for (size_t i = 0; i < n; i++) {
    const char *p = strchr(alphabet, name[i]);
    if (!p)
      return false;
    value = (value << 6) | (unsigned)(p - alphabet);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (k >= USERNAME_MAX_BYTES)
        return false;
      username[k++] = (char)((value >> bits) & 255);
      if (!username[k - 1])
        return false;
    }
  }
  username[k] = '\0';
  if (!username_valid(username))
    return false;
  char encoded[4 * ((USERNAME_MAX_BYTES + 2) / 3) + 1];
  encode(username, encoded);
  return strlen(encoded) == n && !memcmp(encoded, name, n);
}
static AppResult visit_account(const char *name, FileAccessKind kind,
                               void *context) {
  char username[USERNAME_MAX_BYTES + 1];
  if (kind != FILE_ACCESS_REGULAR || !account_filename(name, username))
    return APP_OK;
  Account a;
  AppResult r = storage_account_load(username, &a);
  if (r == APP_OK && !account_list_append(context, &a))
    r = APP_ERR_NO_MEMORY;
  return r;
}
AppResult storage_account_list(AccountList *out) {
  if (!out || out->head || out->tail || out->size)
    return APP_ERR_INVALID_ARGUMENT;
  AppResult r = ready();
  if (r == APP_OK)
    r = file_access_visit_directory(accounts_dir, visit_account, out);
  if (r != APP_OK)
    account_list_clear(out);
  return r;
}
AppResult storage_account_count(size_t *out_count) {
  if (!out_count)
    return APP_ERR_INVALID_ARGUMENT;
  AccountList list = {};
  AppResult r = storage_account_list(&list);
  if (r == APP_OK)
    *out_count = list.size;
  account_list_clear(&list);
  return r;
}

typedef struct {
  const char *path;
  unsigned depth;
  char id[STUDENT_ID_LENGTH + 1];
  StudentList *out;
} RecordWalk;
static AppResult visit_record(const char *name, FileAccessKind kind,
                              void *context) {
  RecordWalk *walk = context;
  static const unsigned widths[] = {2, 2, 2, 1, 2};
  static const unsigned offsets[] = {0, 2, 4, 6, 7};
  unsigned width = widths[walk->depth], offset = offsets[walk->depth];
  char part[3] = {};
  if (walk->depth < 4) {
    if (kind != FILE_ACCESS_DIRECTORY || !digits(name, width))
      return APP_OK;
  } else {
    if (kind != FILE_ACCESS_REGULAR || strlen(name) != 7 ||
        strcmp(name + 2, ".json") != 0)
      return APP_OK;
  }
  memcpy(part, name, width);
  if (!digits(part, width))
    return APP_OK;
  RecordWalk next = *walk;
  memcpy(next.id + offset, part, width);
  next.id[offset + width] = '\0';
  if (walk->depth == 4) {
    StudentCardRecord record;
    AppResult r = storage_record_load(next.id, &record);
    if (r == APP_OK && !student_list_append(walk->out, &record))
      r = APP_ERR_NO_MEMORY;
    return r;
  }
  char *path = path_with_suffix(walk->path, name);
  if (!path)
    return APP_ERR_NO_MEMORY;
  next.path = path;
  next.depth++;
  AppResult r = file_access_visit_directory(path, visit_record, &next);
  free(path);
  return r;
}
AppResult storage_record_list(StudentList *out) {
  if (!out || out->head || out->tail || out->size)
    return APP_ERR_INVALID_ARGUMENT;
  AppResult r = ready();
  RecordWalk walk = {.path = students_dir, .depth = 0, .id = {}, .out = out};
  if (r == APP_OK)
    r = file_access_visit_directory(students_dir, visit_record, &walk);
  if (r != APP_OK)
    student_list_clear(out);
  return r;
}

typedef struct {
  const char *name;
  StudentList *out;
} NameQuery;
static AppResult visit_name(const char *id, FileAccessKind kind,
                            void *context) {
  if (kind != FILE_ACCESS_REGULAR || !digits(id, STUDENT_ID_LENGTH))
    return APP_OK;
  NameQuery *q = context;
  StudentCardRecord record;
  AppResult r = storage_record_load(id, &record);
  if (r == APP_ERR_NOT_FOUND)
    return APP_OK;
  if (r != APP_OK)
    return r;
  if (!strcmp(record.name, q->name) && !student_list_append(q->out, &record))
    return APP_ERR_NO_MEMORY;
  return APP_OK;
}
AppResult storage_record_find_by_name(const char *name, StudentList *out) {
  if (!out || out->head || out->tail || out->size ||
      !text_valid(name, NAME_MAX_BYTES, false))
    return APP_ERR_INVALID_ARGUMENT;
  AppResult r = ready();
  if (r != APP_OK)
    return r;
  char *path = name_path(name);
  if (!path)
    return APP_ERR_NO_MEMORY;
  NameQuery query = {name, out};
  /* 只把姓名目录本身缺失当作无命中，不吞掉回调中的存储错误。 */
  FileAccessKind kind;
  r = file_access_kind(path, &kind);
  if (r == APP_ERR_NOT_FOUND)
    r = APP_OK;
  else if (r == APP_OK)
    r = file_access_visit_directory(path, visit_name, &query);
  free(path);
  if (r != APP_OK)
    student_list_clear(out);
  return r;
}
AppResult storage_name_index_append(const char *name, const char *id) {
  if (!text_valid(name, NAME_MAX_BYTES, false) ||
      !digits(id, STUDENT_ID_LENGTH))
    return APP_ERR_INVALID_ARGUMENT;
  AppResult r = ready();
  if (r != APP_OK)
    return r;
  char *dir = name_path(name);
  if (!dir)
    return APP_ERR_NO_MEMORY;
  r = file_access_make_directories(dir);
  char *path = path_with_suffix(dir, id);
  free(dir);
  if (!path)
    return APP_ERR_NO_MEMORY;
  if (r == APP_OK) {
    FileAccessKind kind;
    r = file_access_kind(path, &kind);
    if (r == APP_ERR_NOT_FOUND)
      r = file_access_write(path, nullptr, 0);
    else if (r == APP_OK) {
      if (kind != FILE_ACCESS_REGULAR)
        r = APP_ERR_INVALID_DATA;
      else {
        char *data;
        size_t size;
        r = file_access_read(path, &data, &size);
        if (r == APP_OK) {
          free(data);
          if (size)
            r = APP_ERR_INVALID_DATA;
        }
      }
    }
  }
  free(path);
  return r;
}
