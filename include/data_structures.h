#ifndef CAMP_CARD_DATA_STRUCTURES_H
#define CAMP_CARD_DATA_STRUCTURES_H

#include "models.h"
#include <stddef.h>

/// 列表自有节点，仅供调用方只读遍历；不得自行链接或释放。
typedef struct StudentNode {
  StudentCardRecord value;
  struct StudentNode *next;
} StudentNode;
/// 列表自有节点，仅供调用方只读遍历；不得自行链接或释放。
typedef struct AccountNode {
  Account value;
  struct AccountNode *next;
} AccountNode;
/// 列表自有节点，仅供调用方只读遍历；不得自行链接或释放。
typedef struct StringNode {
  char *value;
  struct StringNode *next;
} StringNode;
/// 拥有全部节点及数据副本，不可浅复制后分别 clear；不得自行修改链接或 size。
typedef struct {
  StudentNode *head, *tail;
  size_t size;
} StudentList;
/// 拥有全部节点及数据副本，不可浅复制后分别 clear；不得自行修改链接或 size。
typedef struct {
  AccountNode *head, *tail;
  size_t size;
} AccountList;
/// 拥有全部节点及数据副本，不可浅复制后分别 clear；不得自行修改链接或 size。
typedef struct {
  StringNode *head, *tail;
  size_t size;
} StringList;

/// 初始化空表；不释放已有节点，NULL 安全返回。
void student_list_init(StudentList *list);
/// O(1) 追加模型副本，不去重；参数为空或分配失败返回 false，列表不变。
bool student_list_append(StudentList *list, const StudentCardRecord *value);
/// O(n) 查找首个匹配学号；未找到或参数为空返回 NULL。返回借用指针，节点删除或
/// clear 后失效，不得 free。
StudentCardRecord *student_list_find_by_id(const StudentList *list,
                                           const char *student_id);
/// O(n) 删除首个匹配节点；未找到或参数为空返回 false。
bool student_list_remove_by_id(StudentList *list, const char *student_id);
/// O(1) 返回节点数量；NULL 返回 0。
size_t student_list_count(const StudentList *list);
/// O(n) 释放全部自有节点及数据，重置 head、tail、size；可重复调用，NULL 安全。
void student_list_clear(StudentList *list);

/// 初始化空表；不释放已有节点，NULL 安全返回。
void account_list_init(AccountList *list);
/// O(1) 追加账号副本，不去重；参数为空或分配失败返回 false，列表不变。
bool account_list_append(AccountList *list, const Account *value);
/// O(n) 查找首个匹配用户名；未找到或参数为空返回 NULL。返回借用指针，节点删除或
/// clear 后失效，不得 free。
Account *account_list_find_by_username(const AccountList *list, const char *username);
/// O(n) 删除首个匹配节点；未找到或参数为空返回 false。
bool account_list_remove_by_username(AccountList *list, const char *username);
/// O(1) 返回节点数量；NULL 返回 0。
size_t account_list_count(const AccountList *list);
/// O(n) 释放全部自有节点及数据，重置 head、tail、size；可重复调用，NULL 安全。
void account_list_clear(AccountList *list);

/// 初始化空表；不释放已有节点，NULL 安全返回。
void string_list_init(StringList *list);
/// O(n) 按字节去重并深复制 NUL 结尾字符串；重复返回
/// true，参数为空或分配失败返回 false 且列表不变。
bool string_list_append_unique(StringList *list, const char *value);
/// O(n) 按字节比较字符串；不存在或参数为空返回 false。
bool string_list_contains(const StringList *list, const char *value);
/// O(1) 返回节点数量；NULL 返回 0。
size_t string_list_count(const StringList *list);
/// O(n) 释放全部自有节点及数据，重置 head、tail、size；可重复调用，NULL 安全。
void string_list_clear(StringList *list);

#endif // CAMP_CARD_DATA_STRUCTURES_H
