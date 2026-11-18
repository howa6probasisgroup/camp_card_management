#ifndef CAMP_CARD_STORAGE_H
#define CAMP_CARD_STORAGE_H

#include "data_structures.h"
#include <stddef.h>

/**
 * @brief 全量姓名索引检查结果。
 * @details 按索引文件计数；共用同一目录的不同姓名仍按学号区分索引项。
 */
typedef struct {
  /**
   * @brief 缺失索引项数。
   * @details 对应有效主记录，但按姓名和学号定位不到索引文件。
   */
  size_t missing;
  /**
   * @brief 过期索引项数。
   * @details 仅统计形状合规的空索引文件。
   *          主记录不存在或其索引路径已不指向该文件时计入。
   */
  size_t stale;
} NameIndexReport;

/**
 * @brief 复制 data_dir 并创建 accounts、students、indexes/name。
 * @details 相对路径基于启动目录，活动期间不改变工作目录；再次初始化须先
 * shutdown。
 * @param[in] data_dir 数据根目录路径；成功后由存储层复制并在活动期间保留。
 * @return `APP_OK` 表示存储已初始化；参数无效返回
 * `APP_ERR_INVALID_ARGUMENT`，目录创建失败返回 `APP_ERR_IO`，分配失败返回
 * `APP_ERR_NO_MEMORY`。
 */
AppResult storage_init(const char *data_dir);
/**
 * @brief 按区分大小写的用户名直接读取账号。
 * @details 文件名使用原始用户名 ASCII 字节的小写十六进制加 `.json`。
 *          文件中的用户名须与 `username` 逐字节一致。须先初始化存储。
 * @param[in] username 要读取的账号用户名，也是存储路径主键。
 * @param[out] out 成功时接收通过模型校验的账号记录。
 * @return `APP_OK` 表示读取成功。账号不存在返回 `APP_ERR_NOT_FOUND`。
 *         参数无效返回 `APP_ERR_INVALID_ARGUMENT`。
 *         数据损坏返回 `APP_ERR_INVALID_DATA`。
 *         系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_account_load(const char *username, Account *out);
/**
 * @brief 按用户名创建或覆盖账号。
 * @details 路径规则同 `storage_account_load`。
 *          覆盖前须校验已有账号及其主键。
 *          业务层负责查重；直接写入，不保证回滚。须先初始化存储。
 * @param[in] account 要创建或覆盖保存的合法账号记录；重复账号检查由业务层负责。
 * @return `APP_OK` 表示保存成功。参数无效返回 `APP_ERR_INVALID_ARGUMENT`。
 *         数据损坏返回 `APP_ERR_INVALID_DATA`。
 *         系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_account_save(const Account *account);
/**
 * @brief 按用户名删除账号文件。
 * @details 路径规则与 `storage_account_load` 相同。删除前须校验账号及其主键。
 *          不删除学生记录。须先初始化存储。
 * @param[in] username 要删除的账号用户名。
 * @return `APP_OK` 表示删除成功。账号不存在返回 `APP_ERR_NOT_FOUND`。
 *         参数无效返回 `APP_ERR_INVALID_ARGUMENT`。
 *         数据损坏返回 `APP_ERR_INVALID_DATA`。
 *         系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_account_delete(const char *username);
/**
 * @brief 无序枚举账号。
 * @details 只接受有效用户名按 `storage_account_load` 规则生成的规范文件名。
 *          遇到合法路径上的损坏记录即失败并清空结果。须先初始化存储。
 * @param[in,out] out 须初始化为空；由调用方用 `account_list_clear` 释放。
 * @return `APP_OK` 表示枚举成功。记录损坏返回 `APP_ERR_INVALID_DATA`；
 *         失败时列表保持为空。
 */
AppResult storage_account_list(AccountList *out);
/**
 * @brief 按学号直接定位主记录，不扫描学生；
 * @details 成功写入 out，校验模型及路径主键。学号路径按 2-2-2-1-2 分组。
 * 须先初始化存储；无效参数返回 `APP_ERR_INVALID_ARGUMENT`，损坏数据返回
 * `APP_ERR_INVALID_DATA`。 文件不存在返回 `APP_ERR_NOT_FOUND`，系统读写失败返回
 * `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in] student_id 要读取的学生学号，也是主记录路径主键。
 * @param[out] out 成功时接收通过模型和路径主键校验的学生校园卡记录。
 * @return `APP_OK` 表示记录已写入 out；记录不存在返回
 * `APP_ERR_NOT_FOUND`，参数无效返回 `APP_ERR_INVALID_ARGUMENT`，数据损坏返回
 *         `APP_ERR_INVALID_DATA`，系统错误返回 `APP_ERR_IO`，分配失败返回
 * `APP_ERR_NO_MEMORY`。
 */
AppResult storage_record_load(const char *student_id, StudentCardRecord *out);
/**
 * @brief 创建或覆盖合法主记录。
 * @details 余额为 JSON 整数分，状态为约定字符串。直接写入，不修改姓名索引，
 *          无原子提交或回滚。须先初始化存储。
 * @param[in] record 要创建或覆盖保存的合法学生校园卡主记录。
 * @return `APP_OK` 表示保存成功。参数无效返回 `APP_ERR_INVALID_ARGUMENT`。
 *         数据损坏返回 `APP_ERR_INVALID_DATA`。
 *         系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_record_save(const StudentCardRecord *record);
/**
 * @brief out 须初始化为空；
 * @details 无序枚举主记录，成功后调用方
 * `student_list_clear`，遇损坏记录即失败并清理全部结果。
 * 须先初始化存储；无效参数返回 `APP_ERR_INVALID_ARGUMENT`，损坏数据返回
 * `APP_ERR_INVALID_DATA`。 文件不存在返回 `APP_ERR_NOT_FOUND`，系统读写失败返回
 * `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[in,out] out 调用前初始化为空的学生列表；成功后由调用方调用
 * `student_list_clear` 释放。
 * @return `APP_OK` 表示列表构造成功；遇到损坏记录返回
 * `APP_ERR_INVALID_DATA`，其他失败时 out 保持为空。
 */
AppResult storage_record_list(StudentList *out);
/**
 * @brief 按姓名精确查询主记录。
 * @details 仅枚举以学号命名的普通文件。按原始 UTF-8 字节核对主记录姓名，
 *          跳过主记录缺失或姓名不符的候选项。
 *          查询只读，不触发全量扫描或修复。索引缺失时结果可能不全。
 *          须先初始化存储。
 * @param[in] name 要精确匹配的学生姓名。
 * @param[in,out] out 须初始化为空；由调用方用 `student_list_clear` 释放。
 * @return `APP_OK` 表示查询成功，无匹配时返回空表。
 *         参数无效返回 `APP_ERR_INVALID_ARGUMENT`。
 *         主记录损坏返回 `APP_ERR_INVALID_DATA`。
 *         系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_record_find_by_name(const char *name, StudentList *out);
/**
 * @brief 确保单条主记录的姓名索引项存在。
 * @details 核对主记录中的姓名，只检查对应的索引项。
 *          以原始 UTF-8 字节的无填充 Base64URL 构造目录，两个字符一级。
 *          末尾单字符单独一级。索引文件以学号命名，已有空文件时不写入。
 *          此操作可在开卡后调用，也可独立补建已知缺项。须先初始化存储。
 * @param[in] name 要建立索引的学生姓名。
 * @param[in] student_id 姓名对应的学生学号及主记录路径主键。
 * @return `APP_OK` 表示索引项存在。
 *         参数无效或姓名不符返回 `APP_ERR_INVALID_ARGUMENT`。
 *         主记录不存在返回 `APP_ERR_NOT_FOUND`。
 *         主记录损坏或索引项被占用返回 `APP_ERR_INVALID_DATA`。
 *         系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_name_index_append(const char *name, const char *student_id);
/**
 * @brief 只读检查全部姓名索引。
 * @details 根据主记录和文件系统实际解析的目录判断缺项与过期项。
 *          须先初始化存储；检查期间调用方须保证数据目录不受其他操作修改。
 * @param[out] out 成功时接收检查结果；失败时保持原值。
 * @return `APP_OK` 表示检查完成，即使存在缺项或过期项。
 *         参数无效返回 `APP_ERR_INVALID_ARGUMENT`。
 *         主记录损坏或应有索引路径被占用返回 `APP_ERR_INVALID_DATA`。
 *         系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_name_index_check(NameIndexReport *out);
/**
 * @brief 显式修复全部姓名索引。
 * @details 检查规则同 `storage_name_index_check`。
 *          预检通过后，先补缺项，再删除过期空索引文件。
 *          只修改姓名索引；不跟随符号链接，不删除目录，不回滚已完成的修改。
 *          须先初始化存储；修复期间调用方须保证数据目录不受其他操作修改。
 * @return `APP_OK` 表示修复完成。
 *         主记录损坏或应有索引路径被占用返回 `APP_ERR_INVALID_DATA`。
 *         系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_name_index_repair();
/**
 * @brief 统计有效账号，成功写入 out_count；
 * @details 损坏账号返回 `APP_ERR_INVALID_DATA`，不能当作无账号触发初始化。
 * 须先初始化存储；无效参数返回 `APP_ERR_INVALID_ARGUMENT`，损坏数据返回
 * `APP_ERR_INVALID_DATA`。 文件不存在返回 `APP_ERR_NOT_FOUND`，系统读写失败返回
 * `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 * @param[out] out_count 成功时接收有效账号数量；失败时保持原值。
 * @return `APP_OK` 表示数量已写入 out_count；数据损坏返回
 * `APP_ERR_INVALID_DATA`，参数无效返回
 * `APP_ERR_INVALID_ARGUMENT`，文件不存在返回 `APP_ERR_NOT_FOUND`，系统错误返回
 * `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult storage_account_count(size_t *out_count);
/**
 * @brief 幂等释放内存资源，不执行额外持久化。
 * @details 仅释放进程内存储状态，不改写数据文件；重复调用不会造成额外副作用。
 * @return `APP_OK` 表示进程内存储资源已释放；重复调用仍返回 `APP_OK`。
 */
AppResult storage_shutdown();

#endif // CAMP_CARD_STORAGE_H
