#ifndef CAMP_CARD_FILE_ACCESS_H
#define CAMP_CARD_FILE_ACCESS_H

#include "common.h"
#include <stddef.h>

/// 文件系统条目类型。
typedef enum {
  FILE_ACCESS_REGULAR,
  FILE_ACCESS_DIRECTORY,
  FILE_ACCESS_OTHER
} FileAccessKind;
/// 回调借用单个条目名（不是完整路径），仅在回调期间有效；不含 . 和 ..。
/// 任何非 APP_OK 回调结果原样终止并传播。枚举无序、不递归、不跟随符号链接。
typedef AppResult (*FileAccessVisitor)(const char *name, FileAccessKind kind,
                                       void *context);
/// 递归创建；已存在目录成功，同名非目录 IO。
AppResult file_access_make_directories(const char *path);
/// 不存在返回 NOT_FOUND；其他系统错误 IO。
AppResult file_access_kind(const char *path, FileAccessKind *out);
/// 无序、非递归枚举，不跟随符号链接；回调错误原样传播。不包含 . 和
/// ..，不存在返回 APP_ERR_NOT_FOUND，系统错误返回 APP_ERR_IO。
AppResult file_access_visit_directory(const char *path,
                                      FileAccessVisitor visitor, void *context);
/// join 不校验业务键；拒绝绝对子路径及含分隔符的单个 component。
/// 输出不允许与输入重叠；容量不足 INVALID_ARGUMENT，失败输出不变。
AppResult file_access_join(const char *directory, const char *component,
                           char *out, size_t capacity);
/// read 分配 size+1 字节并补 NUL（size 不含该 NUL），不转换内容；
/// 空文件成功。失败输出保持原值；成功使用 file_access_free 释放。
AppResult file_access_read(const char *path, char **out_data, size_t *out_size);
/// 释放 file_access_read 返回的内存；NULL 安全。
void file_access_free(void *data);
/// 直接创建/截断写，size=0 可传 NULL data；检查关闭错误；不原子写入。
AppResult file_access_write(const char *path, const void *data, size_t size);
/// 只删除普通文件；不存在返回 APP_ERR_NOT_FOUND，系统错误返回 APP_ERR_IO。
AppResult file_access_remove(const char *path);

#endif // CAMP_CARD_FILE_ACCESS_H
