#ifndef CAMP_CARD_DATA_STRUCTURES_H
#define CAMP_CARD_DATA_STRUCTURES_H

#include "models.h"
#include <stddef.h>

/**
 * @brief 列表自有节点，仅供调用方只读遍历；
 * @details 不得自行链接或释放。
 */
typedef struct StudentNode {
  StudentCardRecord value;
  struct StudentNode *next;
} StudentNode;
/**
 * @brief 列表自有节点，仅供调用方只读遍历；
 * @details 不得自行链接或释放。
 */
typedef struct AccountNode {
  Account value;
  struct AccountNode *next;
} AccountNode;
/**
 * @brief 列表自有节点，仅供调用方只读遍历；
 * @details 不得自行链接或释放。
 */
typedef struct StringNode {
  char *value;
  struct StringNode *next;
} StringNode;
/**
 * @brief 拥有全部节点及数据副本，不可浅复制后分别 clear；
 * @details 不得自行修改链接或 size。
 */
typedef struct {
  StudentNode *head, *tail;
  size_t size;
} StudentList;
/**
 * @brief 拥有全部节点及数据副本，不可浅复制后分别 clear；
 * @details 不得自行修改链接或 size。
 */
typedef struct {
  AccountNode *head, *tail;
  size_t size;
} AccountList;
/**
 * @brief 拥有全部节点及数据副本，不可浅复制后分别 clear；
 * @details 不得自行修改链接或 size。
 */
typedef struct {
  StringNode *head, *tail;
  size_t size;
} StringList;

/**
 * @brief 初始化空表；
 * @details 不释放已有节点，NULL 安全返回。
 * @param[out] list 要初始化为空的学生列表；已有节点不会在初始化时释放。
 */
void student_list_init(StudentList *list);
/**
 * @brief `O(1)` 追加模型副本，不去重；
 * @details 参数为空或分配失败返回 false，列表不变。
 * @param[in,out] list 已初始化的学生列表；成功后拥有新节点及记录副本。
 * @param[in] value 要复制并追加的学生校园卡记录。
 * @return true 表示副本已追加；false 表示参数无效或分配失败。
 */
bool student_list_append(StudentList *list, const StudentCardRecord *value);
/**
 * @brief `O(n)` 查找首个匹配学号；
 * @details 未找到或参数为空返回 NULL。返回借用指针，节点删除或 clear 后失效，不得 free。
 * @param[in] list 要搜索的学生列表。
 * @param[in] student_id 用于精确匹配的学号。
 * @return 返回首个匹配记录的借用指针；未找到或参数无效时返回 NULL。
 */
StudentCardRecord *student_list_find_by_id(const StudentList *list,
                                           const char *student_id);
/**
 * @brief `O(n)` 删除首个匹配节点；
 * @details 未找到或参数为空返回 false。
 * @param[in,out] list 要删除节点的学生列表。
 * @param[in] student_id 待删除记录的学号。
 * @return true 表示已删除匹配节点；false 表示未找到或参数无效。
 */
bool student_list_remove_by_id(StudentList *list, const char *student_id);
/**
 * @brief `O(1)` 返回节点数量；
 * @details NULL 返回 0。
 * @param[in] list 要读取数量的学生列表。
 * @return 返回列表节点数；list 为 NULL 时返回 0。
 */
size_t student_list_count(const StudentList *list);
/**
 * @brief `O(n)` 释放全部自有节点及数据，重置 head、tail、size；
 * @details 可重复调用，NULL 安全。
 * @param[in,out] list 要释放并重置为空的学生列表。
 */
void student_list_clear(StudentList *list);

/**
 * @brief 初始化空表；
 * @details 不释放已有节点，NULL 安全返回。
 * @param[out] list 要初始化为空的账号列表；已有节点不会在初始化时释放。
 */
void account_list_init(AccountList *list);
/**
 * @brief `O(1)` 追加账号副本，不去重；
 * @details 参数为空或分配失败返回 false，列表不变。
 * @param[in,out] list 已初始化的账号列表；成功后拥有新节点及账号副本。
 * @param[in] value 要复制并追加的账号。
 * @return true 表示副本已追加；false 表示参数无效或分配失败。
 */
bool account_list_append(AccountList *list, const Account *value);
/**
 * @brief `O(n)` 查找首个匹配用户名；
 * @details 未找到或参数为空返回 NULL。返回借用指针，节点删除或 clear 后失效，不得 free。
 * @param[in] list 要搜索的账号列表。
 * @param[in] username 用于精确匹配的用户名。
 * @return 返回首个匹配账号的借用指针；未找到或参数无效时返回 NULL。
 */
Account *account_list_find_by_username(const AccountList *list,
                                       const char *username);
/**
 * @brief `O(n)` 删除首个匹配节点；
 * @details 未找到或参数为空返回 false。
 * @param[in,out] list 要删除节点的账号列表。
 * @param[in] username 待删除账号的用户名。
 * @return true 表示已删除匹配节点；false 表示未找到或参数无效。
 */
bool account_list_remove_by_username(AccountList *list, const char *username);
/**
 * @brief `O(1)` 返回节点数量；
 * @details NULL 返回 0。
 * @param[in] list 要读取数量的账号列表。
 * @return 返回列表节点数；list 为 NULL 时返回 0。
 */
size_t account_list_count(const AccountList *list);
/**
 * @brief `O(n)` 释放全部自有节点及数据，重置 head、tail、size；
 * @details 可重复调用，NULL 安全。
 * @param[in,out] list 要释放并重置为空的账号列表。
 */
void account_list_clear(AccountList *list);

/**
 * @brief 初始化空表；
 * @details 不释放已有节点，NULL 安全返回。
 * @param[out] list 要初始化为空的字符串列表；已有节点不会在初始化时释放。
 */
void string_list_init(StringList *list);
/**
 * @brief `O(n)` 按字节去重并深复制 NUL 结尾字符串；
 * @details 重复返回 true，参数为空或分配失败返回 false 且列表不变。
 * @param[in,out] list 已初始化的字符串列表；仅在字符串尚不存在时追加副本。
 * @param[in] value 以 NUL 结尾、要去重并复制的字符串。
 * @return true 表示字符串已存在或其副本已追加；false 表示参数无效或分配失败。
 */
bool string_list_append_unique(StringList *list, const char *value);
/**
 * @brief `O(n)` 按字节比较字符串；
 * @details 不存在或参数为空返回 false。
 * @param[in] list 要搜索的字符串列表。
 * @param[in] value 以 NUL 结尾、用于按字节比较的字符串。
 * @return true 表示存在按字节相同的字符串；false 表示不存在或参数无效。
 */
bool string_list_contains(const StringList *list, const char *value);
/**
 * @brief `O(1)` 返回节点数量；
 * @details NULL 返回 0。
 * @param[in] list 要读取数量的字符串列表。
 * @return 返回列表节点数；list 为 NULL 时返回 0。
 */
size_t string_list_count(const StringList *list);
/**
 * @brief `O(n)` 释放全部自有节点及数据，重置 head、tail、size；
 * @details 可重复调用，NULL 安全。
 * @param[in,out] list 要释放并重置为空的字符串列表。
 */
void string_list_clear(StringList *list);

#endif // CAMP_CARD_DATA_STRUCTURES_H
