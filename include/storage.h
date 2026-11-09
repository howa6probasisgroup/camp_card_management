#ifndef CAMP_CARD_STORAGE_H
#define CAMP_CARD_STORAGE_H

#include "data_structures.h"
#include <stddef.h>

/**
 * @brief 复制 data_dir 并创建 accounts、students、indexes/name。
 * @details 相对路径基于启动目录，活动期间不改变工作目录；再次初始化须先 shutdown。
 * @param[in] data_dir 数据根目录路径；成功后由存储层复制并在活动期间保留。
 * @return `APP_OK` 表示存储已初始化；参数无效返回 `APP_ERR_INVALID_ARGUMENT`，目录创建失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_init(const char *data_dir);
/**
 * @brief 按用户名路径读取账号，成功写入 out；
 * @details 模型须合法，文件主键须与路径一致。 须先初始化存储；无效参数返回 `APP_ERR_INVALID_ARGUMENT`，损坏数据返回
 *          `APP_ERR_INVALID_DATA`。 文件不存在返回 `APP_ERR_NOT_FOUND`，系统读写失败返回 `APP_ERR_IO`，分配失败返回
 *          `APP_ERR_NO_MEMORY`。
 * @param[in] username 要读取的账号用户名，也是存储路径主键。
 * @param[out] out 成功时接收通过模型校验的账号记录。
 * @return `APP_OK` 表示账号已写入 out；账号不存在返回 `APP_ERR_NOT_FOUND`，参数无效返回 `APP_ERR_INVALID_ARGUMENT`，数据损坏返回
 *         `APP_ERR_INVALID_DATA`，系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_account_load(const char *username, Account *out);
/**
 * @brief 创建或覆盖合法账号；
 * @details 业务层负责查重。统一使用 yyjson 序列化为 UTF-8 无 BOM JSON，角色使用约定字符串；直接写入，不保证回滚。 须先初始化存储；无效参数返回
 *          `APP_ERR_INVALID_ARGUMENT`，损坏数据返回 `APP_ERR_INVALID_DATA`。 文件不存在返回
 *          `APP_ERR_NOT_FOUND`，系统读写失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in] account 要创建或覆盖保存的合法账号记录；重复账号检查由业务层负责。
 * @return `APP_OK` 表示账号已保存；参数无效返回 `APP_ERR_INVALID_ARGUMENT`，数据损坏返回 `APP_ERR_INVALID_DATA`，文件或系统错误返回
 *         `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_account_save(const Account *account);
/**
 * @brief 按用户名删除账号文件；
 * @details 不删除学生记录。 须先初始化存储；无效参数返回 `APP_ERR_INVALID_ARGUMENT`，损坏数据返回 `APP_ERR_INVALID_DATA`。
 *          文件不存在返回 `APP_ERR_NOT_FOUND`，系统读写失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in] username 要删除的账号用户名。
 * @return `APP_OK` 表示账号文件已删除；账号不存在返回 `APP_ERR_NOT_FOUND`，参数无效返回 `APP_ERR_INVALID_ARGUMENT`，数据损坏返回
 *         `APP_ERR_INVALID_DATA`，系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_account_delete(const char *username);
/**
 * @brief out 须初始化为空；
 * @details 无序枚举有效账号，成功后调用方 `account_list_clear`，遇损坏记录即失败并清理全部结果。 须先初始化存储；无效参数返回
 *          `APP_ERR_INVALID_ARGUMENT`，损坏数据返回 `APP_ERR_INVALID_DATA`。 文件不存在返回
 *          `APP_ERR_NOT_FOUND`，系统读写失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in,out] out 调用前初始化为空的账号列表；成功后由调用方调用 `account_list_clear` 释放。
 * @return `APP_OK` 表示列表构造成功；遇到损坏记录返回 `APP_ERR_INVALID_DATA`，其他失败时 out 保持为空。
 */
AppResult storage_account_list(AccountList *out);
/**
 * @brief 按学号直接定位主记录，不扫描学生；
 * @details 成功写入 out，校验模型及路径主键。学号路径按 2-2-2-1-2 分组。 须先初始化存储；无效参数返回
 *          `APP_ERR_INVALID_ARGUMENT`，损坏数据返回 `APP_ERR_INVALID_DATA`。 文件不存在返回
 *          `APP_ERR_NOT_FOUND`，系统读写失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in] student_id 要读取的学生学号，也是主记录路径主键。
 * @param[out] out 成功时接收通过模型和路径主键校验的学生校园卡记录。
 * @return `APP_OK` 表示记录已写入 out；记录不存在返回 `APP_ERR_NOT_FOUND`，参数无效返回 `APP_ERR_INVALID_ARGUMENT`，数据损坏返回
 *         `APP_ERR_INVALID_DATA`，系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_record_load(const char *student_id, StudentCardRecord *out);
/**
 * @brief 创建或覆盖合法主记录；
 * @details 统一使用 yyjson 序列化，余额为 JSON 整数分，状态为约定字符串。直接写入，无原子提交或回滚。 须先初始化存储；无效参数返回
 *          `APP_ERR_INVALID_ARGUMENT`，损坏数据返回 `APP_ERR_INVALID_DATA`。 文件不存在返回
 *          `APP_ERR_NOT_FOUND`，系统读写失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in] record 要创建或覆盖保存的合法学生校园卡主记录。
 * @return `APP_OK` 表示记录已保存；参数无效返回 `APP_ERR_INVALID_ARGUMENT`，数据损坏返回 `APP_ERR_INVALID_DATA`，文件或系统错误返回
 *         `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_record_save(const StudentCardRecord *record);
/**
 * @brief out 须初始化为空；
 * @details 无序枚举主记录，成功后调用方 `student_list_clear`，遇损坏记录即失败并清理全部结果。 须先初始化存储；无效参数返回
 *          `APP_ERR_INVALID_ARGUMENT`，损坏数据返回 `APP_ERR_INVALID_DATA`。 文件不存在返回
 *          `APP_ERR_NOT_FOUND`，系统读写失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in,out] out 调用前初始化为空的学生列表；成功后由调用方调用 `student_list_clear` 释放。
 * @return `APP_OK` 表示列表构造成功；遇到损坏记录返回 `APP_ERR_INVALID_DATA`，其他失败时 out 保持为空。
 */
AppResult storage_record_list(StudentList *out);
/**
 * @brief 姓名索引非权威；
 * @details 仅跳过非法索引文件名、失效学号和姓名不符记录； 主记录损坏或 `APP_ERR_IO` 错误必须传播；姓名目录不存在、为空或无有效命中均返回 `APP_OK` 空表。
 *          须先初始化存储；无效参数返回 `APP_ERR_INVALID_ARGUMENT`，损坏数据返回 `APP_ERR_INVALID_DATA`。
 *          单个主记录不存在时跳过，系统读写失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in] name 要精确匹配的学生姓名。
 * @param[in,out] out 调用前初始化为空的学生列表；成功后由调用方调用 `student_list_clear` 释放。
 * @return `APP_OK` 表示查询完成（无有效命中时为空表）；参数无效返回 `APP_ERR_INVALID_ARGUMENT`，主记录损坏返回
 *         `APP_ERR_INVALID_DATA`，系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_record_find_by_name(const char *name, StudentList *out);
/**
 * @brief 已存在的空索引项返回 `APP_OK`；
 * @details 先保存主记录，再追加索引。 学号路径 2-2-2-1-2；用户名 Base64URL 无填充；姓名同样编码后每两字符分层。
 *          索引文件名为学号，内容为空；已有非空索引或非普通文件返回 `APP_ERR_INVALID_DATA`。 不自动修复或重建。 须先初始化存储；无效参数返回
 *          `APP_ERR_INVALID_ARGUMENT`，损坏数据返回 `APP_ERR_INVALID_DATA`。 文件不存在返回
 *          `APP_ERR_NOT_FOUND`，系统读写失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in] name 要建立索引的学生姓名。
 * @param[in] student_id 姓名对应的学生学号及主记录路径主键。
 * @return `APP_OK` 表示索引已追加；参数无效返回 `APP_ERR_INVALID_ARGUMENT`，索引数据损坏返回 `APP_ERR_INVALID_DATA`，文件不存在返回
 *         `APP_ERR_NOT_FOUND`，系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_name_index_append(const char *name, const char *student_id);
/**
 * @brief 统计有效账号，成功写入 out_count；
 * @details 损坏账号返回 `APP_ERR_INVALID_DATA`，不能当作无账号触发初始化。 须先初始化存储；无效参数返回
 *          `APP_ERR_INVALID_ARGUMENT`，损坏数据返回 `APP_ERR_INVALID_DATA`。 文件不存在返回
 *          `APP_ERR_NOT_FOUND`，系统读写失败返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[out] out_count 成功时接收有效账号数量；失败时保持原值。
 * @return `APP_OK` 表示数量已写入 out_count；数据损坏返回 `APP_ERR_INVALID_DATA`，参数无效返回 `APP_ERR_INVALID_ARGUMENT`，文件不存在返回
 *         `APP_ERR_NOT_FOUND`，系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_account_count(size_t *out_count);
/**
 * @brief 幂等释放内存资源，不执行额外持久化。
 * @details 仅释放进程内存储状态，不改写数据文件；重复调用不会造成额外副作用。
 * @return `APP_OK` 表示进程内存储资源已释放；重复调用仍返回 `APP_OK`。
 */
AppResult storage_shutdown(void);

#endif // CAMP_CARD_STORAGE_H
