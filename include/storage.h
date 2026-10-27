#ifndef CAMP_CARD_STORAGE_H
#define CAMP_CARD_STORAGE_H

#include "data_structures.h"
#include <stddef.h>

/// 复制 data_dir 并创建
/// accounts、students、indexes/name。相对路径基于启动目录，活动期间不改变工作目录；再次初始化须先
/// shutdown。
AppResult storage_init(const char *data_dir);
/// 按用户名路径读取账号，成功写入 out；模型须合法，文件主键须与路径一致。
/// 须先初始化存储；无效参数返回 APP_ERR_INVALID_ARGUMENT，损坏数据返回
/// APP_ERR_INVALID_DATA。 文件不存在返回 APP_ERR_NOT_FOUND，系统读写失败返回
/// APP_ERR_IO，分配失败返回 APP_ERR_NO_MEMORY。
AppResult storage_account_load(const char *username, Account *out);
/// 创建或覆盖合法账号；业务层负责查重。统一使用 yyjson 序列化为 UTF-8 无 BOM
/// JSON，角色使用约定字符串；直接写入，不保证回滚。
/// 须先初始化存储；无效参数返回 APP_ERR_INVALID_ARGUMENT，损坏数据返回
/// APP_ERR_INVALID_DATA。 文件不存在返回 APP_ERR_NOT_FOUND，系统读写失败返回
/// APP_ERR_IO，分配失败返回 APP_ERR_NO_MEMORY。
AppResult storage_account_save(const Account *account);
/// 按用户名删除账号文件；不删除学生记录。
/// 须先初始化存储；无效参数返回 APP_ERR_INVALID_ARGUMENT，损坏数据返回
/// APP_ERR_INVALID_DATA。 文件不存在返回 APP_ERR_NOT_FOUND，系统读写失败返回
/// APP_ERR_IO，分配失败返回 APP_ERR_NO_MEMORY。
AppResult storage_account_delete(const char *username);
/// out 须初始化为空；无序枚举有效账号，成功后调用方
/// account_list_clear，遇损坏记录即失败并清理全部结果。
/// 须先初始化存储；无效参数返回 APP_ERR_INVALID_ARGUMENT，损坏数据返回
/// APP_ERR_INVALID_DATA。 文件不存在返回 APP_ERR_NOT_FOUND，系统读写失败返回
/// APP_ERR_IO，分配失败返回 APP_ERR_NO_MEMORY。
AppResult storage_account_list(AccountList *out);
/// 按学号直接定位主记录，不扫描学生；成功写入
/// out，校验模型及路径主键。学号路径按 2-2-2-1-2 分组。
/// 须先初始化存储；无效参数返回 APP_ERR_INVALID_ARGUMENT，损坏数据返回
/// APP_ERR_INVALID_DATA。 文件不存在返回 APP_ERR_NOT_FOUND，系统读写失败返回
/// APP_ERR_IO，分配失败返回 APP_ERR_NO_MEMORY。
AppResult storage_record_load(const char *student_id, StudentCardRecord *out);
/// 创建或覆盖合法主记录；统一使用 yyjson 序列化，余额为 JSON
/// 整数分，状态为约定字符串。直接写入，无原子提交或回滚。
/// 须先初始化存储；无效参数返回 APP_ERR_INVALID_ARGUMENT，损坏数据返回
/// APP_ERR_INVALID_DATA。 文件不存在返回 APP_ERR_NOT_FOUND，系统读写失败返回
/// APP_ERR_IO，分配失败返回 APP_ERR_NO_MEMORY。
AppResult storage_record_save(const StudentCardRecord *record);
/// out 须初始化为空；无序枚举主记录，成功后调用方
/// student_list_clear，遇损坏记录即失败并清理全部结果。
/// 须先初始化存储；无效参数返回 APP_ERR_INVALID_ARGUMENT，损坏数据返回
/// APP_ERR_INVALID_DATA。 文件不存在返回 APP_ERR_NOT_FOUND，系统读写失败返回
/// APP_ERR_IO，分配失败返回 APP_ERR_NO_MEMORY。
AppResult storage_record_list(StudentList *out);
/// 姓名索引非权威；仅跳过非法索引文件名、失效学号和姓名不符记录；
/// 主记录损坏或 IO 错误必须传播；姓名目录不存在、为空或无有效命中均返回 APP_OK
/// 空表。 须先初始化存储；无效参数返回 APP_ERR_INVALID_ARGUMENT，损坏数据返回
/// APP_ERR_INVALID_DATA。 单个主记录不存在时跳过，系统读写失败返回
/// APP_ERR_IO，分配失败返回 APP_ERR_NO_MEMORY。
AppResult storage_record_find_by_name(const char *name, StudentList *out);
/// 已存在的空索引项返回 APP_OK；先保存主记录，再追加索引。
/// 学号路径 2-2-2-1-2；用户名 Base64URL 无填充；姓名同样编码后每两字符分层。
/// 索引文件名为学号，内容为空；已有非空索引或非普通文件返回
/// APP_ERR_INVALID_DATA。 不自动修复或重建。 须先初始化存储；无效参数返回
/// APP_ERR_INVALID_ARGUMENT，损坏数据返回 APP_ERR_INVALID_DATA。 文件不存在返回
/// APP_ERR_NOT_FOUND，系统读写失败返回 APP_ERR_IO，分配失败返回
/// APP_ERR_NO_MEMORY。
AppResult storage_name_index_append(const char *name, const char *student_id);
/// 统计有效账号，成功写入 out_count；损坏账号返回
/// APP_ERR_INVALID_DATA，不能当作无账号触发初始化。
/// 须先初始化存储；无效参数返回 APP_ERR_INVALID_ARGUMENT，损坏数据返回
/// APP_ERR_INVALID_DATA。 文件不存在返回 APP_ERR_NOT_FOUND，系统读写失败返回
/// APP_ERR_IO，分配失败返回 APP_ERR_NO_MEMORY。
AppResult storage_account_count(size_t *out_count);
/// 幂等释放内存资源，不执行额外持久化。
AppResult storage_shutdown(void);

#endif // CAMP_CARD_STORAGE_H
