#include "data_structures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 数据结构模块的独立测试。覆盖空表、追加顺序、头/中/尾查找与删除、重复
// clear、字符串去重与深复制、模型副本独立性。分配失败路径不做故障注入，
// 由代码审查保证失败时列表不变且仍可释放。

static size_t g_checks;
static size_t g_failures;

static void check(const bool condition, const char *expression,
                  const int line) {
  g_checks++;
  if (!condition) {
    g_failures++;
    fprintf(stderr, "FAIL line %d: %s\n", line, expression);
  }
}

#define CHECK(condition) check((condition), #condition, __LINE__)

static void set_text(char *destination, const size_t capacity,
                     const char *source) {
  const size_t length = strlen(source);
  if (length >= capacity) {
    fprintf(stderr, "test setup error: text does not fit buffer\n");
    exit(EXIT_FAILURE);
  }
  memcpy(destination, source, length + 1);
}

static StudentCardRecord make_student(const char *student_id, const char *name,
                                      const int64_t balance_cents,
                                      const CardStatus status) {
  StudentCardRecord record;
  memset(&record, 0, sizeof record);
  set_text(record.student_id, sizeof record.student_id, student_id);
  set_text(record.name, sizeof record.name, name);
  set_text(record.sex, sizeof record.sex, "男");
  set_text(record.department, sizeof record.department, "计算机学院");
  set_text(record.class_name, sizeof record.class_name, "24154B1");
  set_text(record.phone, sizeof record.phone, "13800000000");
  record.balance_cents = balance_cents;
  record.status = status;
  return record;
}

static Account make_account(const char *username, const UserRole role,
                            const char *student_id) {
  Account account;
  memset(&account, 0, sizeof account);
  set_text(account.username, sizeof account.username, username);
  set_text(account.password_salt_hex, sizeof account.password_salt_hex,
           "00112233445566778899aabbccddeeff");
  // 测试用派生值占位，共 64 个小写十六进制字符；不对应真实密码。
  set_text(account.password_hash_hex, sizeof account.password_hash_hex,
           "0123456789abcdef0123456789abcdef"
           "0123456789abcdef0123456789abcdef");
  account.role = role;
  set_text(account.student_id, sizeof account.student_id, student_id);
  return account;
}

static bool student_equals(const StudentCardRecord *left,
                           const StudentCardRecord *right) {
  return strcmp(left->student_id, right->student_id) == 0 &&
         strcmp(left->name, right->name) == 0 &&
         strcmp(left->sex, right->sex) == 0 &&
         strcmp(left->department, right->department) == 0 &&
         strcmp(left->class_name, right->class_name) == 0 &&
         strcmp(left->phone, right->phone) == 0 &&
         left->balance_cents == right->balance_cents &&
         left->status == right->status;
}

static bool account_equals(const Account *left, const Account *right) {
  return strcmp(left->username, right->username) == 0 &&
         strcmp(left->password_salt_hex, right->password_salt_hex) == 0 &&
         strcmp(left->password_hash_hex, right->password_hash_hex) == 0 &&
         left->role == right->role &&
         strcmp(left->student_id, right->student_id) == 0;
}

// 校验链表计数、尾指针和节点数一致；同时能发现意外断链或环。
static bool student_list_is_sound(const StudentList *list,
                                  const size_t expected) {
  if (list->size != expected || student_list_count(list) != expected) {
    return false;
  }
  if (list->head == nullptr || list->tail == nullptr) {
    return expected == 0 && list->head == nullptr && list->tail == nullptr;
  }
  if (list->tail->next != nullptr) {
    return false;
  }
  const StudentNode *last = nullptr;
  size_t walked = 0;
  for (const StudentNode *node = list->head; node != nullptr;
       node = node->next) {
    last = node;
    walked++;
    if (walked > expected) {
      return false;
    }
  }
  return walked == expected && last == list->tail;
}

static bool account_list_is_sound(const AccountList *list,
                                  const size_t expected) {
  if (list->size != expected || account_list_count(list) != expected) {
    return false;
  }
  if (list->head == nullptr || list->tail == nullptr) {
    return expected == 0 && list->head == nullptr && list->tail == nullptr;
  }
  if (list->tail->next != nullptr) {
    return false;
  }
  const AccountNode *last = nullptr;
  size_t walked = 0;
  for (const AccountNode *node = list->head; node != nullptr;
       node = node->next) {
    last = node;
    walked++;
    if (walked > expected) {
      return false;
    }
  }
  return walked == expected && last == list->tail;
}

static bool string_list_is_sound(const StringList *list,
                                 const size_t expected) {
  if (list->size != expected || string_list_count(list) != expected) {
    return false;
  }
  if (list->head == nullptr || list->tail == nullptr) {
    return expected == 0 && list->head == nullptr && list->tail == nullptr;
  }
  if (list->tail->next != nullptr) {
    return false;
  }
  const StringNode *last = nullptr;
  size_t walked = 0;
  for (const StringNode *node = list->head; node != nullptr;
       node = node->next) {
    last = node;
    walked++;
    if (walked > expected) {
      return false;
    }
  }
  return walked == expected && last == list->tail;
}

static bool student_ids_are(const StudentList *list, const char *const *ids,
                            const size_t count) {
  if (student_list_count(list) != count) {
    return false;
  }
  const StudentNode *node = list->head;
  for (size_t i = 0; i < count; i++) {
    if (node == nullptr || strcmp(node->value.student_id, ids[i]) != 0) {
      return false;
    }
    node = node->next;
  }
  return node == nullptr;
}

static bool account_usernames_are(const AccountList *list,
                                  const char *const *usernames,
                                  const size_t count) {
  if (account_list_count(list) != count) {
    return false;
  }
  const AccountNode *node = list->head;
  for (size_t i = 0; i < count; i++) {
    if (node == nullptr || strcmp(node->value.username, usernames[i]) != 0) {
      return false;
    }
    node = node->next;
  }
  return node == nullptr;
}

static bool string_values_are(const StringList *list, const char *const *values,
                              const size_t count) {
  if (string_list_count(list) != count) {
    return false;
  }
  const StringNode *node = list->head;
  for (size_t i = 0; i < count; i++) {
    if (node == nullptr || strcmp(node->value, values[i]) != 0) {
      return false;
    }
    node = node->next;
  }
  return node == nullptr;
}

static void test_student_list_empty_and_null_safety() {
  StudentList list;
  student_list_init(&list);
  CHECK(list.head == nullptr);
  CHECK(list.tail == nullptr);
  CHECK(list.size == 0);
  CHECK(student_list_count(&list) == 0);
  CHECK(student_list_find_by_id(&list, "251543201") == nullptr);
  CHECK(student_list_remove_by_id(&list, "251543201") == false);
  student_list_clear(&list);
  student_list_clear(&list);
  CHECK(student_list_is_sound(&list, 0));

  // 空指针安全：所有接口都不得解引用空指针。
  student_list_init(nullptr);
  CHECK(student_list_append(nullptr, nullptr) == false);
  CHECK(student_list_find_by_id(nullptr, "251543201") == nullptr);
  CHECK(student_list_remove_by_id(nullptr, "251543201") == false);
  CHECK(student_list_count(nullptr) == 0);
  student_list_clear(nullptr);

  const StudentCardRecord record =
      make_student("251543201", "张三", 0, CARD_STATUS_ACTIVE);
  CHECK(student_list_append(nullptr, &record) == false);
  CHECK(student_list_append(&list, nullptr) == false);
  CHECK(student_list_find_by_id(&list, nullptr) == nullptr);
  CHECK(student_list_remove_by_id(&list, nullptr) == false);
  CHECK(student_list_is_sound(&list, 0));
}

static void test_student_list_append_and_order() {
  StudentList list;
  student_list_init(&list);
  const StudentCardRecord first =
      make_student("251543201", "张三", 0, CARD_STATUS_ACTIVE);
  const StudentCardRecord second =
      make_student("251543202", "李四", 1250, CARD_STATUS_FROZEN);
  const StudentCardRecord third =
      make_student("251543203", "王五", INT64_MAX, CARD_STATUS_ACTIVE);

  CHECK(student_list_append(&list, &first));
  CHECK(student_list_append(&list, &second));
  CHECK(student_list_append(&list, &third));
  CHECK(student_list_is_sound(&list, 3));
  CHECK(strcmp(list.head->value.student_id, "251543201") == 0);
  CHECK(strcmp(list.tail->value.student_id, "251543203") == 0);
  CHECK(list.tail->next == nullptr);

  const char *expected[] = {"251543201", "251543202", "251543203"};
  CHECK(student_ids_are(&list, expected, 3));
  CHECK(student_equals(&list.head->value, &first));
  CHECK(student_equals(&list.head->next->value, &second));
  CHECK(student_equals(&list.tail->value, &third));

  student_list_clear(&list);
}

static void test_student_list_find() {
  StudentList list;
  student_list_init(&list);
  const StudentCardRecord first =
      make_student("251543201", "张三", 0, CARD_STATUS_ACTIVE);
  const StudentCardRecord second =
      make_student("251543202", "李四", 1250, CARD_STATUS_FROZEN);
  const StudentCardRecord third =
      make_student("251543203", "王五", 99, CARD_STATUS_ACTIVE);
  student_list_append(&list, &first);
  student_list_append(&list, &second);
  student_list_append(&list, &third);

  const StudentCardRecord *found = student_list_find_by_id(&list, "251543201");
  CHECK(found != nullptr && student_equals(found, &first));
  found = student_list_find_by_id(&list, "251543202");
  CHECK(found != nullptr && student_equals(found, &second));
  found = student_list_find_by_id(&list, "251543203");
  CHECK(found != nullptr && student_equals(found, &third));
  CHECK(student_list_find_by_id(&list, "251543204") == nullptr);
  CHECK(student_list_find_by_id(&list, "25154320") == nullptr);
  CHECK(student_list_find_by_id(&list, "") == nullptr);

  // 链表允许重复学号；查找返回第一个匹配节点，删除后返回下一个。
  const StudentCardRecord duplicate =
      make_student("251543202", "同名副本", 7, CARD_STATUS_ACTIVE);
  CHECK(student_list_append(&list, &duplicate));
  found = student_list_find_by_id(&list, "251543202");
  CHECK(found != nullptr && strcmp(found->name, "李四") == 0);
  CHECK(student_list_remove_by_id(&list, "251543202"));
  found = student_list_find_by_id(&list, "251543202");
  CHECK(found != nullptr && student_equals(found, &duplicate));

  student_list_clear(&list);
}

static void test_student_list_remove() {
  StudentList list;
  student_list_init(&list);
  const StudentCardRecord first =
      make_student("251543201", "张三", 0, CARD_STATUS_ACTIVE);
  const StudentCardRecord second =
      make_student("251543202", "李四", 1250, CARD_STATUS_ACTIVE);
  const StudentCardRecord third =
      make_student("251543203", "王五", 99, CARD_STATUS_ACTIVE);
  student_list_append(&list, &first);
  student_list_append(&list, &second);
  student_list_append(&list, &third);

  // 删除中间节点：头尾不变，链表仍可遍历。
  CHECK(student_list_remove_by_id(&list, "251543202"));
  const char *after_middle[] = {"251543201", "251543203"};
  CHECK(student_list_is_sound(&list, 2));
  CHECK(student_ids_are(&list, after_middle, 2));
  CHECK(strcmp(list.tail->value.student_id, "251543203") == 0);

  // 删除不存在元素：返回 false 且结构不变。
  CHECK(student_list_remove_by_id(&list, "999999999") == false);
  CHECK(student_list_is_sound(&list, 2));
  CHECK(student_ids_are(&list, after_middle, 2));

  // 删除头节点：head 前移。
  CHECK(student_list_remove_by_id(&list, "251543201"));
  const char *after_head[] = {"251543203"};
  CHECK(student_list_is_sound(&list, 1));
  CHECK(student_ids_are(&list, after_head, 1));
  CHECK(list.head == list.tail);

  // 删除唯一节点：头和尾同时回到 nullptr。
  CHECK(student_list_remove_by_id(&list, "251543203"));
  CHECK(student_list_is_sound(&list, 0));
  CHECK(list.head == nullptr && list.tail == nullptr);
  CHECK(student_list_remove_by_id(&list, "251543203") == false);

  // 清空后继续追加，尾指针必须正确建立。
  CHECK(student_list_append(&list, &second));
  CHECK(student_list_is_sound(&list, 1));
  CHECK(student_list_append(&list, &third));
  CHECK(student_list_is_sound(&list, 2));

  // 删除尾节点后尾指针回退，再追加仍能正确链接。
  CHECK(student_list_remove_by_id(&list, "251543203"));
  CHECK(student_list_is_sound(&list, 1));
  CHECK(strcmp(list.tail->value.student_id, "251543202") == 0);
  CHECK(student_list_append(&list, &first));
  CHECK(student_list_is_sound(&list, 2));
  const char *after_append[] = {"251543202", "251543201"};
  CHECK(student_ids_are(&list, after_append, 2));

  student_list_clear(&list);
}

static void test_student_list_clear_and_reuse() {
  StudentList list;
  student_list_init(&list);
  const StudentCardRecord record =
      make_student("251543201", "张三", 0, CARD_STATUS_ACTIVE);
  CHECK(student_list_append(&list, &record));
  student_list_clear(&list);
  CHECK(student_list_is_sound(&list, 0));
  CHECK(list.head == nullptr && list.tail == nullptr);
  CHECK(student_list_find_by_id(&list, "251543201") == nullptr);
  student_list_clear(&list);
  CHECK(student_list_is_sound(&list, 0));

  CHECK(student_list_append(&list, &record));
  CHECK(student_list_is_sound(&list, 1));
  student_list_clear(&list);
}

static void test_student_list_copies_values() {
  StudentList list;
  student_list_init(&list);
  StudentCardRecord original =
      make_student("251543201", "张三", 1200, CARD_STATUS_ACTIVE);
  CHECK(student_list_append(&list, &original));

  // 修改调用方结构体不得影响节点内的副本。
  set_text(original.name, sizeof original.name, "赵六");
  original.balance_cents = 9999;
  original.status = CARD_STATUS_FROZEN;
  StudentCardRecord *found = student_list_find_by_id(&list, "251543201");
  CHECK(found != nullptr && strcmp(found->name, "张三") == 0);
  CHECK(found != nullptr && found->balance_cents == 1200);
  CHECK(found != nullptr && found->status == CARD_STATUS_ACTIVE);

  // find 返回列表内部副本的借用指针，再次查找可见同一节点。
  CHECK(found != nullptr);
  if (found != nullptr) {
    found->balance_cents = 1350;
  }
  found = student_list_find_by_id(&list, "251543201");
  CHECK(found != nullptr && found->balance_cents == 1350);

  student_list_clear(&list);
}

static void test_account_list() {
  AccountList list;
  account_list_init(&list);
  CHECK(account_list_count(&list) == 0);
  CHECK(account_list_find_by_username(&list, "admin") == nullptr);
  CHECK(account_list_remove_by_username(&list, "admin") == false);
  account_list_clear(&list);
  account_list_clear(&list);
  CHECK(account_list_is_sound(&list, 0));

  account_list_init(nullptr);
  CHECK(account_list_append(nullptr, nullptr) == false);
  CHECK(account_list_find_by_username(nullptr, "admin") == nullptr);
  CHECK(account_list_remove_by_username(nullptr, "admin") == false);
  CHECK(account_list_count(nullptr) == 0);
  account_list_clear(nullptr);

  const Account admin = make_account("admin", ROLE_ADMIN, "");
  const Account clerk = make_account("clerk01", ROLE_CARD_CLERK, "");
  const Account user = make_account("user001", ROLE_CARD_USER, "251543201");
  CHECK(account_list_append(&list, &admin));
  CHECK(account_list_append(&list, &clerk));
  CHECK(account_list_append(&list, &user));
  CHECK(account_list_append(&list, nullptr) == false);
  CHECK(account_list_is_sound(&list, 3));

  const char *expected[] = {"admin", "clerk01", "user001"};
  CHECK(account_usernames_are(&list, expected, 3));
  CHECK(account_equals(&list.head->value, &admin));
  CHECK(account_equals(&list.tail->value, &user));

  const Account *found = account_list_find_by_username(&list, "admin");
  CHECK(found != nullptr && account_equals(found, &admin));
  found = account_list_find_by_username(&list, "clerk01");
  CHECK(found != nullptr && account_equals(found, &clerk));
  found = account_list_find_by_username(&list, "user001");
  CHECK(found != nullptr && account_equals(found, &user));
  CHECK(found != nullptr && found->role == ROLE_CARD_USER);
  CHECK(found != nullptr && strcmp(found->student_id, "251543201") == 0);
  CHECK(account_list_find_by_username(&list, "missing") == nullptr);
  CHECK(account_list_find_by_username(&list, "") == nullptr);

  // 删除中间、头部、尾部节点后结构与顺序仍然正确。
  CHECK(account_list_remove_by_username(&list, "clerk01"));
  CHECK(account_list_is_sound(&list, 2));
  CHECK(account_list_remove_by_username(&list, "admin"));
  CHECK(account_list_is_sound(&list, 1));
  CHECK(account_list_remove_by_username(&list, "user001"));
  CHECK(account_list_is_sound(&list, 0));
  CHECK(account_list_remove_by_username(&list, "user001") == false);

  // 副本独立性：修改调用方结构体不影响节点内容。
  Account copy = make_account("user002", ROLE_CARD_USER, "251543202");
  CHECK(account_list_append(&list, &copy));
  set_text(copy.username, sizeof copy.username, "changed");
  set_text(copy.student_id, sizeof copy.student_id, "999999999");
  CHECK(account_list_find_by_username(&list, "user002") != nullptr);
  CHECK(account_list_find_by_username(&list, "changed") == nullptr);

  account_list_clear(&list);
  CHECK(account_list_is_sound(&list, 0));
}

static void test_string_list() {
  StringList list;
  string_list_init(&list);
  CHECK(string_list_count(&list) == 0);
  CHECK(string_list_contains(&list, "251543201") == false);
  CHECK(string_list_append_unique(&list, "251543201"));
  CHECK(string_list_count(&list) == 1);
  CHECK(string_list_contains(&list, "251543201"));

  // 重复值不重复插入，但仍返回成功。
  CHECK(string_list_append_unique(&list, "251543201"));
  CHECK(string_list_count(&list) == 1);

  CHECK(string_list_append_unique(&list, "251543202"));
  CHECK(string_list_append_unique(&list, "251543203"));
  const char *expected[] = {"251543201", "251543202", "251543203"};
  CHECK(string_values_are(&list, expected, 3));
  CHECK(string_list_is_sound(&list, 3));
  CHECK(string_list_contains(&list, "251543202"));
  CHECK(string_list_contains(&list, "251543203"));
  CHECK(string_list_contains(&list, "251543204") == false);
  CHECK(string_list_contains(&list, nullptr) == false);
  CHECK(string_list_contains(nullptr, "251543201") == false);
  CHECK(string_list_append_unique(&list, nullptr) == false);
  CHECK(string_list_append_unique(nullptr, "251543201") == false);
  CHECK(string_list_count(nullptr) == 0);
  string_list_clear(nullptr);
  CHECK(string_list_is_sound(&list, 3));

  // 空字符串是合法的 NUL 结尾字符串。
  CHECK(string_list_append_unique(&list, ""));
  CHECK(string_list_contains(&list, ""));
  CHECK(string_list_append_unique(&list, ""));
  CHECK(string_list_is_sound(&list, 4));

  // 深复制：修改调用方缓冲区后节点内容不变。
  char buffer[16];
  set_text(buffer, sizeof buffer, "251543204");
  CHECK(string_list_append_unique(&list, buffer));
  set_text(buffer, sizeof buffer, "999999999");
  CHECK(string_list_contains(&list, "251543204"));
  CHECK(string_list_contains(&list, "999999999") == false);

  string_list_clear(&list);
  CHECK(string_list_is_sound(&list, 0));
  string_list_clear(&list);
  CHECK(string_list_is_sound(&list, 0));

  // clear 后可以继续使用，尾指针重新建立。
  CHECK(string_list_append_unique(&list, "251543201"));
  CHECK(string_list_is_sound(&list, 1));
  string_list_clear(&list);
}

static void test_many_operations() {
  enum { NODE_COUNT = 1000 };
  StudentList list;
  student_list_init(&list);
  for (int i = 0; i < NODE_COUNT; i++) {
    char student_id[STUDENT_ID_LENGTH + 1];
    snprintf(student_id, sizeof student_id, "%09d", i);
    StudentCardRecord record =
        make_student(student_id, "批量测试", i, CARD_STATUS_ACTIVE);
    CHECK(student_list_append(&list, &record));
  }
  CHECK(student_list_is_sound(&list, NODE_COUNT));

  // 删除全部偶数编号节点，验证中间删除后计数与顺序递进正确。
  for (int i = 0; i < NODE_COUNT; i += 2) {
    char student_id[STUDENT_ID_LENGTH + 1];
    snprintf(student_id, sizeof student_id, "%09d", i);
    CHECK(student_list_remove_by_id(&list, student_id));
  }
  CHECK(student_list_is_sound(&list, NODE_COUNT / 2));
  size_t index = 0;
  for (const StudentNode *node = list.head; node != nullptr;
       node = node->next) {
    char expected_id[STUDENT_ID_LENGTH + 1];
    snprintf(expected_id, sizeof expected_id, "%09d", (int)(index * 2 + 1));
    CHECK(strcmp(node->value.student_id, expected_id) == 0);
    index++;
  }
  CHECK(index == NODE_COUNT / 2);

  student_list_clear(&list);
  CHECK(student_list_is_sound(&list, 0));
}

int main() {
  test_student_list_empty_and_null_safety();
  test_student_list_append_and_order();
  test_student_list_find();
  test_student_list_remove();
  test_student_list_clear_and_reuse();
  test_student_list_copies_values();
  test_account_list();
  test_string_list();
  test_many_operations();

  if (g_failures == 0) {
    printf("test_data_structures: %zu checks passed\n", g_checks);
    return EXIT_SUCCESS;
  }
  fprintf(stderr, "test_data_structures: %zu of %zu checks failed\n",
          g_failures, g_checks);
  return EXIT_FAILURE;
}
