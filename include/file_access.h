#ifndef CAMP_CARD_FILE_ACCESS_H
#define CAMP_CARD_FILE_ACCESS_H

#include "common.h"
#include <stddef.h>

/**
 * @brief 文件系统条目类型。
 * @details `FILE_ACCESS_REGULAR`、`FILE_ACCESS_DIRECTORY` 和 `FILE_ACCESS_OTHER` 分别表示普通文件、目录和其他文件系统项。
 */
typedef enum {
  /**
   * @brief 普通文件。
   * @details 表示目录项是可读写的常规文件类型。
   */
  FILE_ACCESS_REGULAR,
  /**
   * @brief 目录。
   * @details 表示目录项可用于目录遍历。
   */
  FILE_ACCESS_DIRECTORY,
  /**
   * @brief 其他文件系统项。
   * @details 包括不属于普通文件或目录的条目。
   */
  FILE_ACCESS_OTHER
} FileAccessKind;
/**
 * @brief 回调借用单个条目名（不是完整路径），仅在回调期间有效；
 * @details 不含 . 和 ..。 任何非 `APP_OK` 回调结果原样终止并传播。枚举无序、不递归、不跟随符号链接。
 * @param[in] name 当前条目的名称，不含完整路径；仅在回调期间有效。
 * @param[in] kind 当前条目的文件系统类型。
 * @param[in,out] context 调用方提供的上下文指针，可用于传递或更新遍历状态。
 * @return 返回 `APP_OK` 表示继续枚举；任何其他结果会终止枚举并由 `file_access_visit_directory` 原样传播。
 */
typedef AppResult (*FileAccessVisitor)(const char *name, FileAccessKind kind,
                                       void *context);
/**
 * @brief 递归创建；
 * @details 已存在目录成功，同名非目录 `APP_ERR_IO`。
 * @param[in] path 要递归创建的目录路径。
 * @return `APP_OK` 表示目录已创建或原本存在；同名非目录或其他系统错误返回 `APP_ERR_IO`。
 */
AppResult file_access_make_directories(const char *path);
/**
 * @brief 不存在返回 `APP_ERR_NOT_FOUND`；
 * @details 其他系统错误 `APP_ERR_IO`。
 * @param[in] path 要查询类型的文件系统路径。
 * @param[out] out 成功时接收路径对应的文件系统条目类型。
 * @return `APP_OK` 表示 out 已写入类型；路径不存在返回 `APP_ERR_NOT_FOUND`，其他系统错误返回 `APP_ERR_IO`。
 */
AppResult file_access_kind(const char *path, FileAccessKind *out);
/**
 * @brief 无序、非递归枚举，不跟随符号链接；
 * @details 回调错误原样传播。不包含 . 和 ..，不存在返回 `APP_ERR_NOT_FOUND`，系统错误返回 `APP_ERR_IO`。
 * @param[in] path 要枚举的目录路径。
 * @param[in] visitor 处理每个目录条目的回调函数。
 * @param[in,out] context 传递给每次回调的调用方上下文指针。
 * @return `APP_OK` 表示枚举完成；目录不存在返回 `APP_ERR_NOT_FOUND`，系统错误返回 `APP_ERR_IO`，回调错误原样传播。
 */
AppResult file_access_visit_directory(const char *path,
                                      FileAccessVisitor visitor, void *context);
/**
 * @brief join 不校验业务键；
 * @details 拒绝绝对子路径及含分隔符的单个 component。 输出不允许与输入重叠；容量不足 `APP_ERR_INVALID_ARGUMENT`，失败输出不变。
 * @param[in] directory 作为路径前缀的目录。
 * @param[in] component 单个路径组件；不得含路径分隔符。
 * @param[out] out 接收拼接后路径的缓冲区；不得与输入重叠。
 * @param[in] capacity out 缓冲区容量，单位为字节，包含末尾 NUL 所需空间。
 * @return `APP_OK` 表示拼接成功；无效路径组件、缓冲区重叠或容量不足返回 `APP_ERR_INVALID_ARGUMENT`。
 */
AppResult file_access_join(const char *directory, const char *component,
                           char *out, size_t capacity);
/**
 * @brief read 分配 size+1 字节并补 NUL（size 不含该 NUL），不转换内容；
 * @details 空文件成功。失败输出保持原值；成功使用 `file_access_free` 释放。
 * @param[in] path 要读取的普通文件路径。
 * @param[out] out_data 成功时接收新分配并以 NUL 结尾的数据；使用 `file_access_free` 释放。
 * @param[out] out_size 成功时接收文件原始字节数，不包含补加的 NUL。
 * @return `APP_OK` 表示读取成功；文件不存在返回 `APP_ERR_NOT_FOUND`，系统错误返回 `APP_ERR_IO`，分配失败返回 `APP_ERR_NO_MEMORY`。
 */
AppResult file_access_read(const char *path, char **out_data, size_t *out_size);
/**
 * @brief 释放 `file_access_read` 返回的内存；
 * @details NULL 安全。
 * @param[in] data 由 `file_access_read` 返回、需要释放的数据指针；允许为 NULL。
 */
void file_access_free(void *data);
/**
 * @brief 直接创建/截断写，size=0 可传 NULL data；
 * @details 检查关闭错误；不原子写入。
 * @param[in] path 要创建或截断写入的文件路径。
 * @param[in] data 要写入的字节数据；size 为零时允许为 NULL。
 * @param[in] size 要写入的字节数。
 * @return `APP_OK` 表示写入成功；系统读写或关闭失败返回 `APP_ERR_IO`。
 */
AppResult file_access_write(const char *path, const void *data, size_t size);
/**
 * @brief 只删除普通文件；
 * @details 不存在返回 `APP_ERR_NOT_FOUND`，系统错误返回 `APP_ERR_IO`。
 * @param[in] path 要删除的普通文件路径。
 * @return `APP_OK` 表示删除成功；文件不存在返回 `APP_ERR_NOT_FOUND`，系统错误返回 `APP_ERR_IO`。
 */
AppResult file_access_remove(const char *path);

#endif // CAMP_CARD_FILE_ACCESS_H
