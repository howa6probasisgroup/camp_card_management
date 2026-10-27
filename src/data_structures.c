#include "data_structures.h"

#include <stdlib.h>
#include <string.h>

// 三种链表都用尾指针维护 O(1) 追加。节点和节点内的数据副本归列表所有，
// 删除或清空时释放节点；StringList 额外释放字符串副本。

void student_list_init(StudentList *list) {
  if (list == nullptr) {
    return;
  }
  // 不释放已有节点：重复初始化非空列表属于调用方错误，会泄漏节点。
  list->head = nullptr;
  list->tail = nullptr;
  list->size = 0;
}

bool student_list_append(StudentList *list, const StudentCardRecord *value) {
  if (list == nullptr || value == nullptr) {
    return false;
  }
  StudentNode *node = malloc(sizeof *node);
  if (node == nullptr) {
    return false;
  }
  // 按值复制模型：调用方随后修改或释放原结构体不影响列表内容。
  node->value = *value;
  node->next = nullptr;
  if (list->tail != nullptr) {
    list->tail->next = node;
  } else {
    list->head = node;
  }
  list->tail = node;
  list->size++;
  return true;
}

StudentCardRecord *student_list_find_by_id(const StudentList *list,
                                           const char *student_id) {
  if (list == nullptr || student_id == nullptr) {
    return nullptr;
  }
  for (StudentNode *node = list->head; node != nullptr; node = node->next) {
    if (strcmp(node->value.student_id, student_id) == 0) {
      return &node->value;
    }
  }
  return nullptr;
}

bool student_list_remove_by_id(StudentList *list, const char *student_id) {
  if (list == nullptr || student_id == nullptr) {
    return false;
  }
  StudentNode *previous = nullptr;
  StudentNode *current = list->head;
  while (current != nullptr) {
    if (strcmp(current->value.student_id, student_id) == 0) {
      if (previous == nullptr) {
        list->head = current->next;
      } else {
        previous->next = current->next;
      }
      // 删除尾节点时必须回退尾指针；删除中间节点时尾指针不变。
      if (current == list->tail) {
        list->tail = previous;
      }
      free(current);
      list->size--;
      return true;
    }
    previous = current;
    current = current->next;
  }
  return false;
}

size_t student_list_count(const StudentList *list) {
  return list == nullptr ? 0 : list->size;
}

void student_list_clear(StudentList *list) {
  if (list == nullptr) {
    return;
  }
  StudentNode *current = list->head;
  while (current != nullptr) {
    StudentNode *next = current->next;
    free(current);
    current = next;
  }
  list->head = nullptr;
  list->tail = nullptr;
  list->size = 0;
}

void account_list_init(AccountList *list) {
  if (list == nullptr) {
    return;
  }
  // 不释放已有节点：重复初始化非空列表属于调用方错误，会泄漏节点。
  list->head = nullptr;
  list->tail = nullptr;
  list->size = 0;
}

bool account_list_append(AccountList *list, const Account *value) {
  if (list == nullptr || value == nullptr) {
    return false;
  }
  AccountNode *node = malloc(sizeof *node);
  if (node == nullptr) {
    return false;
  }
  // Account 只含定长字符数组和枚举，按值复制即可，不会与调用方共享内存。
  node->value = *value;
  node->next = nullptr;
  if (list->tail != nullptr) {
    list->tail->next = node;
  } else {
    list->head = node;
  }
  list->tail = node;
  list->size++;
  return true;
}

Account *account_list_find_by_username(const AccountList *list,
                                       const char *username) {
  if (list == nullptr || username == nullptr) {
    return nullptr;
  }
  for (AccountNode *node = list->head; node != nullptr; node = node->next) {
    if (strcmp(node->value.username, username) == 0) {
      return &node->value;
    }
  }
  return nullptr;
}

bool account_list_remove_by_username(AccountList *list, const char *username) {
  if (list == nullptr || username == nullptr) {
    return false;
  }
  AccountNode *previous = nullptr;
  AccountNode *current = list->head;
  while (current != nullptr) {
    if (strcmp(current->value.username, username) == 0) {
      if (previous == nullptr) {
        list->head = current->next;
      } else {
        previous->next = current->next;
      }
      if (current == list->tail) {
        list->tail = previous;
      }
      free(current);
      list->size--;
      return true;
    }
    previous = current;
    current = current->next;
  }
  return false;
}

size_t account_list_count(const AccountList *list) {
  return list == nullptr ? 0 : list->size;
}

void account_list_clear(AccountList *list) {
  if (list == nullptr) {
    return;
  }
  AccountNode *current = list->head;
  while (current != nullptr) {
    AccountNode *next = current->next;
    free(current);
    current = next;
  }
  list->head = nullptr;
  list->tail = nullptr;
  list->size = 0;
}

void string_list_init(StringList *list) {
  if (list == nullptr) {
    return;
  }
  // 不释放已有节点：重复初始化非空列表属于调用方错误，会泄漏节点。
  list->head = nullptr;
  list->tail = nullptr;
  list->size = 0;
}

bool string_list_append_unique(StringList *list, const char *value) {
  if (list == nullptr || value == nullptr) {
    return false;
  }
  for (const StringNode *node = list->head; node != nullptr;
       node = node->next) {
    if (strcmp(node->value, value) == 0) {
      return true;
    }
  }

  const size_t length = strlen(value);
  char *copy = malloc(length + 1);
  if (copy == nullptr) {
    return false;
  }
  memcpy(copy, value, length + 1);

  StringNode *node = malloc(sizeof *node);
  if (node == nullptr) {
    // 先分配字符串副本再分配节点时，节点分配失败必须释放副本，列表保持不变。
    free(copy);
    return false;
  }
  node->value = copy;
  node->next = nullptr;
  if (list->tail != nullptr) {
    list->tail->next = node;
  } else {
    list->head = node;
  }
  list->tail = node;
  list->size++;
  return true;
}

bool string_list_contains(const StringList *list, const char *value) {
  if (list == nullptr || value == nullptr) {
    return false;
  }
  for (const StringNode *node = list->head; node != nullptr;
       node = node->next) {
    if (strcmp(node->value, value) == 0) {
      return true;
    }
  }
  return false;
}

size_t string_list_count(const StringList *list) {
  return list == nullptr ? 0 : list->size;
}

void string_list_clear(StringList *list) {
  if (list == nullptr) {
    return;
  }
  StringNode *current = list->head;
  while (current != nullptr) {
    StringNode *next = current->next;
    free(current->value);
    free(current);
    current = next;
  }
  list->head = nullptr;
  list->tail = nullptr;
  list->size = 0;
}
