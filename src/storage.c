#include "storage.h"
#include "file_access.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <yyjson.h>

static char *accounts_dir, *students_dir, *names_dir;
static constexpr char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
static const char *roles[] = {"admin", "card_user", "card_clerk"};

static size_t bounded_length(const char *s, const size_t max) {
  size_t n = 0;
  if (!s)
    return max + 1;
  while (n <= max && s[n])
    n++;
  return n;
}

static bool digits(const char *s, const size_t n) {
  if (bounded_length(s, n) != n)
    return false;
  for (size_t i = 0; i < n; i++)
    if (s[i] < '0' || s[i] > '9')
      return false;
  return true;
}

static bool username_valid(const char *s) {
  const size_t n = bounded_length(s, USERNAME_MAX_BYTES);
  if (!n || n > USERNAME_MAX_BYTES)
    return false;
  for (size_t i = 0; i < n; i++) {
    const char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
      return false;
  }
  return true;
}

static bool hex_valid(const char *s, const size_t n) {
  if (bounded_length(s, n) != n)
    return false;
  for (size_t i = 0; i < n; i++)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
      return false;
  return true;
}

/* 严格 UTF-8：排除过长编码、代理项及超出 Unicode 上限的码点。 */
static bool text_valid(const char *s, const size_t max, const bool empty) {
  const size_t n = bounded_length(s, max);
  if (n > max || (!empty && !n))
    return false;
  for (size_t i = 0; i < n;) {
    const unsigned char c = (unsigned char)s[i++];
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
  const auto s = (const unsigned char *)text;
  const size_t n = strlen(text);
  size_t k = 0;
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

static char *duplicate_string(const char *s) {
  if (!s)
    return nullptr;
  const size_t n = strlen(s);
  char *copy = malloc(n + 1);
  if (copy)
    memcpy(copy, s, n + 1);
  return copy;
}

static char *path_with_suffix(const char *root, const char *suffix) {
  const size_t a = strlen(root);
  const size_t b = strlen(suffix);
  if (a > SIZE_MAX - b - 2)
    return nullptr;
  char *p = malloc(a + b + 2);
  if (p)
    snprintf(p, a + b + 2, "%s/%s", root, suffix);
  return p;
}

static void hex_encode_username(const char *username, char *out) {
  static constexpr char hex_digits[] = "0123456789abcdef";
  size_t k = 0;
  for (size_t i = 0; username[i]; i++) {
    const unsigned char c = (unsigned char)username[i];
    out[k++] = hex_digits[c >> 4];
    out[k++] = hex_digits[c & 0x0f];
  }
  out[k] = '\0';
}

static char *account_path(const char *username) {
  char encoded[2 * USERNAME_MAX_BYTES + 6];
  hex_encode_username(username, encoded);
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

static bool name_index_directory_valid(const char *path) {
  const size_t root_length = strlen(names_dir);
  if (strncmp(path, names_dir, root_length) != 0 || path[root_length] != '/')
    return false;

  const char *component = path + root_length + 1;
  if (!*component)
    return false;

  size_t encoded_length = 0;
  while (*component) {
    size_t component_length = 0;
    while (*component && *component != '/') {
      if (!strchr(alphabet, *component))
        return false;
      component_length++;
      component++;
    }
    if (component_length < 1 || component_length > 2)
      return false;
    encoded_length += component_length;
    if (*component == '/' && (component_length != 2 || !*++component))
      return false;
  }
  return encoded_length >= 2;
}

static AppResult ready() {
  if (!accounts_dir)
    return APP_ERR_INVALID_ARGUMENT;
  const char *dirs[] = {accounts_dir, students_dir, names_dir};
  for (size_t i = 0; i < 3; i++) {
    FileAccessKind kind;
    const AppResult r = file_access_kind(dirs[i], &kind);
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
static bool object_fields(const yyjson_val *obj, const char *const *fields,
                          const size_t count) {
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
    if (i == count || seen & (1u << i))
      return false;
    seen |= 1u << i;
  }
  return seen == (1u << count) - 1;
}

static bool get_text(const yyjson_val *obj, const char *key, char *out,
                     const size_t capacity) {
  const yyjson_val *v = yyjson_obj_get(obj, key);
  if (!yyjson_is_str(v))
    return false;
  const size_t n = yyjson_get_len(v);
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
  const AppResult r = file_access_read(path, &data, &size);
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
  const AppResult result = read_document(path, &doc);
  if (result != APP_OK)
    return result;
  const yyjson_val *obj = yyjson_doc_get_root(doc);
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
  const AppResult result = read_document(path, &doc);
  if (result != APP_OK)
    return result;
  const yyjson_val *obj = yyjson_doc_get_root(doc);
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
  const yyjson_val *balance = yyjson_obj_get(obj, "balance_cents");
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

static AppResult write_document(const char *path, const yyjson_mut_doc *doc,
                                const bool create_parent) {
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
  // 覆盖前核对实际记录的用户名，主键不符时不得截断现有数据。
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
  const bool ok =
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
  const bool ok =
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
  // 删除前核对实际记录的用户名，防止路径指向其他账号。
  Account existing;
  r = account_read_path(path, username, &existing);
  if (r == APP_OK)
    r = file_access_remove(path);
  free(path);
  return r;
}

/* 文件名必须是由有效用户名生成的规范小写十六进制加
 * .json，非规范条目不参与记录。 */
static bool account_filename(const char *name, char *username) {
  const size_t len = strlen(name);
  if (len < 7 || strcmp(name + len - 5, ".json") != 0)
    return false;
  const size_t hex_len = len - 5;
  if (hex_len % 2 != 0 || hex_len > 2 * USERNAME_MAX_BYTES)
    return false;
  size_t k = 0;
  for (size_t i = 0; i < hex_len; i += 2) {
    const char c1 = name[i];
    const char c2 = name[i + 1];
    const int v1 = c1 >= '0' && c1 <= '9'   ? c1 - '0'
                   : c1 >= 'a' && c1 <= 'f' ? c1 - 'a' + 10
                                            : -1;
    const int v2 = c2 >= '0' && c2 <= '9'   ? c2 - '0'
                   : c2 >= 'a' && c2 <= 'f' ? c2 - 'a' + 10
                                            : -1;
    if (v1 < 0 || v2 < 0)
      return false;
    const char byte = (char)((v1 << 4) | v2);
    if (!byte)
      return false;
    username[k++] = byte;
  }
  username[k] = '\0';
  return username_valid(username);
}

static AppResult visit_account(const char *name, const FileAccessKind kind,
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
  const AppResult r = storage_account_list(&list);
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
static AppResult visit_record(const char *name, const FileAccessKind kind,
                              void *context) {
  const RecordWalk *walk = context;
  static const unsigned widths[] = {2, 2, 2, 1, 2};
  static const unsigned offsets[] = {0, 2, 4, 6, 7};
  const unsigned width = widths[walk->depth];
  const unsigned offset = offsets[walk->depth];
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
  const AppResult r = file_access_visit_directory(path, visit_record, &next);
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
static AppResult visit_name(const char *id, const FileAccessKind kind,
                            void *context) {
  if (kind != FILE_ACCESS_REGULAR || !digits(id, STUDENT_ID_LENGTH))
    return APP_OK;
  const NameQuery *q = context;
  StudentCardRecord record;
  const AppResult r = storage_record_load(id, &record);
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
  NameQuery query = {.name = name, .out = out};
  // 只把姓名目录本身缺失当作无命中，不吞掉回调中的存储错误。
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
  StudentCardRecord record;
  r = storage_record_load(id, &record);
  if (r != APP_OK)
    return r;
  if (strcmp(record.name, name) != 0)
    return APP_ERR_INVALID_ARGUMENT;
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

typedef struct ActualIndexNode {
  char *path;
  char *dir;
  char id[STUDENT_ID_LENGTH + 1];
  struct ActualIndexNode *next;
} ActualIndexNode;

typedef struct MissingIndexNode {
  char id[STUDENT_ID_LENGTH + 1];
  char name[NAME_MAX_BYTES + 1];
  struct MissingIndexNode *next;
} MissingIndexNode;

typedef struct StaleIndexNode {
  char *path;
  struct StaleIndexNode *next;
} StaleIndexNode;

typedef struct {
  MissingIndexNode *missing_head;
  size_t missing_count;
  StaleIndexNode *stale_head;
  size_t stale_count;
} NameIndexAnalysis;

static void free_analysis(NameIndexAnalysis *analysis) {
  MissingIndexNode *m = analysis->missing_head;
  while (m) {
    MissingIndexNode *next = m->next;
    free(m);
    m = next;
  }
  StaleIndexNode *s = analysis->stale_head;
  while (s) {
    StaleIndexNode *next = s->next;
    free(s->path);
    free(s);
    s = next;
  }
  analysis->missing_head = nullptr;
  analysis->stale_head = nullptr;
  analysis->missing_count = 0;
  analysis->stale_count = 0;
}

typedef struct {
  const char *dir_path;
  ActualIndexNode **head;
} ActualCollector;

static AppResult collect_actual_indexes(const char *name,
                                        const FileAccessKind kind,
                                        void *context) {
  const ActualCollector *col = context;
  if (kind == FILE_ACCESS_DIRECTORY) {
    char *sub = path_with_suffix(col->dir_path, name);
    if (!sub)
      return APP_ERR_NO_MEMORY;
    ActualCollector next_col = {.dir_path = sub, .head = col->head};
    const AppResult r =
        file_access_visit_directory(sub, collect_actual_indexes, &next_col);
    free(sub);
    return r;
  }
  if (kind == FILE_ACCESS_REGULAR && digits(name, STUDENT_ID_LENGTH) &&
      name_index_directory_valid(col->dir_path)) {
    char *path = path_with_suffix(col->dir_path, name);
    if (!path)
      return APP_ERR_NO_MEMORY;
    char *data = nullptr;
    size_t size = 0;
    const AppResult r = file_access_read(path, &data, &size);
    if (r != APP_OK) {
      free(path);
      return r;
    }
    file_access_free(data);
    if (size == 0) {
      ActualIndexNode *node = malloc(sizeof(*node));
      if (!node) {
        free(path);
        return APP_ERR_NO_MEMORY;
      }
      node->path = path;
      node->dir = duplicate_string(col->dir_path);
      if (!node->dir) {
        free(path);
        free(node);
        return APP_ERR_NO_MEMORY;
      }
      memcpy(node->id, name, sizeof(node->id));
      node->next = *col->head;
      *col->head = node;
      return APP_OK;
    }
    free(path);
  }
  return APP_OK;
}

static AppResult analyze_name_indexes(NameIndexAnalysis *analysis) {
  AppResult r = ready();
  if (r != APP_OK)
    return r;

  StudentList students = {};
  r = storage_record_list(&students);
  if (r != APP_OK)
    return r;

  for (const StudentNode *node = students.head; node; node = node->next) {
    const StudentCardRecord *rec = &node->value;
    char *dir = name_path(rec->name);
    if (!dir) {
      student_list_clear(&students);
      return APP_ERR_NO_MEMORY;
    }
    const size_t prefix_len = strlen(names_dir);
    for (size_t i = prefix_len + 1; dir[i]; i++) {
      if (dir[i] == '/') {
        dir[i] = '\0';
        FileAccessKind pk;
        const AppResult pr = file_access_kind(dir, &pk);
        dir[i] = '/';
        if (pr == APP_OK && pk != FILE_ACCESS_DIRECTORY) {
          free(dir);
          student_list_clear(&students);
          return APP_ERR_INVALID_DATA;
        }
      }
    }
    FileAccessKind dk;
    const AppResult dr = file_access_kind(dir, &dk);
    if (dr == APP_OK && dk != FILE_ACCESS_DIRECTORY) {
      free(dir);
      student_list_clear(&students);
      return APP_ERR_INVALID_DATA;
    }

    char *file_path = path_with_suffix(dir, rec->student_id);
    free(dir);
    if (!file_path) {
      student_list_clear(&students);
      return APP_ERR_NO_MEMORY;
    }
    FileAccessKind fk;
    const AppResult fr = file_access_kind(file_path, &fk);
    if (fr == APP_ERR_NOT_FOUND) {
      MissingIndexNode *m = malloc(sizeof(*m));
      if (!m) {
        free(file_path);
        student_list_clear(&students);
        return APP_ERR_NO_MEMORY;
      }
      memcpy(m->id, rec->student_id, sizeof(m->id));
      memcpy(m->name, rec->name, sizeof(m->name));
      m->next = analysis->missing_head;
      analysis->missing_head = m;
      analysis->missing_count++;
    } else if (fr == APP_OK) {
      if (fk != FILE_ACCESS_REGULAR) {
        free(file_path);
        student_list_clear(&students);
        return APP_ERR_INVALID_DATA;
      }
      char *data = nullptr;
      size_t size = 0;
      r = file_access_read(file_path, &data, &size);
      if (r != APP_OK) {
        free(file_path);
        student_list_clear(&students);
        return r;
      }
      file_access_free(data);
      if (size != 0) {
        free(file_path);
        student_list_clear(&students);
        return APP_ERR_INVALID_DATA;
      }
    } else {
      free(file_path);
      student_list_clear(&students);
      return fr;
    }
    free(file_path);
  }

  ActualIndexNode *actual_head = nullptr;
  ActualCollector col = {names_dir, &actual_head};
  r = file_access_visit_directory(names_dir, collect_actual_indexes, &col);
  if (r != APP_OK && r != APP_ERR_NOT_FOUND) {
    while (actual_head) {
      ActualIndexNode *next = actual_head->next;
      free(actual_head->path);
      free(actual_head->dir);
      free(actual_head);
      actual_head = next;
    }
    student_list_clear(&students);
    return r;
  }

  for (const ActualIndexNode *act = actual_head; act; act = act->next) {
    StudentCardRecord *s = student_list_find_by_id(&students, act->id);
    bool is_stale = false;
    if (!s) {
      is_stale = true;
    } else {
      char *expected_dir = name_path(s->name);
      if (!expected_dir) {
        while (actual_head) {
          ActualIndexNode *next = actual_head->next;
          free(actual_head->path);
          free(actual_head->dir);
          free(actual_head);
          actual_head = next;
        }
        student_list_clear(&students);
        return APP_ERR_NO_MEMORY;
      }
      bool same = false;
      const AppResult sr =
          file_access_same_directory(act->dir, expected_dir, &same);
      free(expected_dir);
      if (sr == APP_ERR_NOT_FOUND) {
        same = false;
      } else if (sr != APP_OK) {
        while (actual_head) {
          ActualIndexNode *next = actual_head->next;
          free(actual_head->path);
          free(actual_head->dir);
          free(actual_head);
          actual_head = next;
        }
        student_list_clear(&students);
        return sr;
      }
      if (!same)
        is_stale = true;
    }

    if (is_stale) {
      StaleIndexNode *st = malloc(sizeof(*st));
      if (!st) {
        while (actual_head) {
          ActualIndexNode *next = actual_head->next;
          free(actual_head->path);
          free(actual_head->dir);
          free(actual_head);
          actual_head = next;
        }
        student_list_clear(&students);
        return APP_ERR_NO_MEMORY;
      }
      st->path = duplicate_string(act->path);
      if (!st->path) {
        free(st);
        while (actual_head) {
          ActualIndexNode *next = actual_head->next;
          free(actual_head->path);
          free(actual_head->dir);
          free(actual_head);
          actual_head = next;
        }
        student_list_clear(&students);
        return APP_ERR_NO_MEMORY;
      }
      st->next = analysis->stale_head;
      analysis->stale_head = st;
      analysis->stale_count++;
    }
  }

  while (actual_head) {
    ActualIndexNode *next = actual_head->next;
    free(actual_head->path);
    free(actual_head->dir);
    free(actual_head);
    actual_head = next;
  }
  student_list_clear(&students);
  return APP_OK;
}

AppResult storage_name_index_check(NameIndexReport *out) {
  if (!out)
    return APP_ERR_INVALID_ARGUMENT;
  NameIndexAnalysis analysis = {};
  const AppResult r = analyze_name_indexes(&analysis);
  if (r == APP_OK) {
    out->missing = analysis.missing_count;
    out->stale = analysis.stale_count;
  }
  free_analysis(&analysis);
  return r;
}

AppResult storage_name_index_repair() {
  NameIndexAnalysis analysis = {};
  AppResult r = analyze_name_indexes(&analysis);
  if (r != APP_OK) {
    free_analysis(&analysis);
    return r;
  }
  for (const MissingIndexNode *m = analysis.missing_head; m; m = m->next) {
    char *dir = name_path(m->name);
    if (!dir) {
      free_analysis(&analysis);
      return APP_ERR_NO_MEMORY;
    }
    r = file_access_make_directories(dir);
    char *path = path_with_suffix(dir, m->id);
    free(dir);
    if (!path) {
      free_analysis(&analysis);
      return APP_ERR_NO_MEMORY;
    }
    if (r == APP_OK)
      r = file_access_write(path, nullptr, 0);
    free(path);
    if (r != APP_OK) {
      free_analysis(&analysis);
      return r;
    }
  }
  for (const StaleIndexNode *s = analysis.stale_head; s; s = s->next) {
    r = file_access_remove(s->path);
    if (r != APP_OK) {
      free_analysis(&analysis);
      return r;
    }
  }
  free_analysis(&analysis);
  return APP_OK;
}
