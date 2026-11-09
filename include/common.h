#ifndef CAMP_CARD_COMMON_H
#define CAMP_CARD_COMMON_H

/**
 * @brief 学号的 ASCII 数字位数。
 * @details 学号字符数组还须为末尾 NUL 预留一个字节；卡号与学号使用相同长度。
 */
#define STUDENT_ID_LENGTH 9
/**
 * @brief 用户名最大字节数，不含末尾 NUL。
 * @details 用户名按字节数校验，并须遵循用户名格式约束；存储数组还须为结尾 NUL 预留一个字节。
 */
#define USERNAME_MAX_BYTES 48
/**
 * @brief 姓名最大 UTF-8 字节数，不含末尾 NUL。
 * @details 按 UTF-8 编码后的字节数限制姓名，存储字符串的数组还须为结尾 NUL 预留一个字节。
 */
#define NAME_MAX_BYTES 96
/**
 * @brief 性别字段最大字节数，不含末尾 NUL。
 * @details 限制性别字符串的最大字节数；数组还须为结尾 NUL 预留一个字节。
 */
#define SEX_MAX_BYTES 15
/**
 * @brief 院系最大 UTF-8 字节数，不含末尾 NUL。
 * @details 按 UTF-8 编码后的字节数限制院系名称，数组还须为结尾 NUL 预留一个字节。
 */
#define DEPARTMENT_MAX_BYTES 96
/**
 * @brief 班级最大 UTF-8 字节数，不含末尾 NUL。
 * @details 按 UTF-8 编码后的字节数限制班级名称，数组还须为结尾 NUL 预留一个字节。
 */
#define CLASS_NAME_MAX_BYTES 64
/**
 * @brief 电话最大字节数，不含末尾 NUL。
 * @details 限制电话号码的最大字节数；数组还须为结尾 NUL 预留一个字节。
 */
#define PHONE_MAX_BYTES 24
/**
 * @brief 密码最小字节数。
 * @details 密码按字节数校验，UTF-8 多字节字符会占用多个字节。
 */
#define PASSWORD_MIN_BYTES 8
/**
 * @brief 密码最大字节数。
 * @details 密码按字节数校验，UTF-8 多字节字符会占用多个字节。
 */
#define PASSWORD_MAX_BYTES 64
/**
 * @brief 随机盐的字节数。
 * @details 该值表示二进制数据的字节数；十六进制文本长度由对应的 HEX_LENGTH 常量约束。
 */
#define PASSWORD_SALT_BYTES 16
/**
 * @brief 密码派生值的字节数。
 * @details 该值表示二进制数据的字节数；十六进制文本长度由对应的 HEX_LENGTH 常量约束。
 */
#define PASSWORD_HASH_BYTES 32
/**
 * @brief 盐的十六进制字符数，不含末尾 NUL。
 * @details 该长度不含字符串末尾 NUL，缓冲区容量应再增加一个字节。
 */
#define PASSWORD_SALT_HEX_LENGTH 32
/**
 * @brief 派生值的十六进制字符数，不含末尾 NUL。
 * @details 该长度不含字符串末尾 NUL，缓冲区容量应再增加一个字节。
 */
#define PASSWORD_HASH_HEX_LENGTH 64
/**
 * @brief PBKDF2-HMAC-SHA-256 迭代次数。
 * @details 创建凭据与校验凭据必须使用相同迭代次数；变更该值会影响已有凭据的兼容性。
 */
#define PASSWORD_PBKDF2_ITERATIONS 200000

/**
 * @brief 统一操作结果；
 * @details 参数指针为空返回 `APP_ERR_INVALID_ARGUMENT`，失败时标量和模型输出保持原值。 模块不打印错误或秘密；由 CLI 根据返回码提示。
 */
typedef enum {
  /**
   * @brief 操作成功。
   * @details 未发生错误，输出参数按对应接口契约写入。
   */
  APP_OK = 0,
  /**
   * @brief 参数无效。
   * @details 参数为空、格式错误或超出接口允许范围时返回。
   */
  APP_ERR_INVALID_ARGUMENT,
  /**
   * @brief 请求的数据不存在。
   * @details 路径或业务主键未找到时返回。
   */
  APP_ERR_NOT_FOUND,
  /**
   * @brief 请求创建的数据已存在。
   * @details 唯一键冲突或禁止覆盖时返回。
   */
  APP_ERR_ALREADY_EXISTS,
  /**
   * @brief 身份认证失败。
   * @details 用户名或密码不匹配时返回。
   */
  APP_ERR_AUTH_FAILED,
  /**
   * @brief 调用者无权执行该操作。
   * @details 角色不允许该操作或目标记录不属于调用者时返回。
   */
  APP_ERR_PERMISSION_DENIED,
  /**
   * @brief 可用余额不足。
   * @details 消费金额大于当前卡余额时返回。
   */
  APP_ERR_INSUFFICIENT_BALANCE,
  /**
   * @brief 校园卡被冻结。
   * @details 冻结卡执行充值或消费等受限资金操作时返回。
   */
  APP_ERR_CARD_FROZEN,
  /**
   * @brief 文件系统操作失败。
   * @details 系统调用无法完成读写、枚举、创建或删除时返回。
   */
  APP_ERR_IO,
  /**
   * @brief 持久化数据格式或内容无效。
   * @details 数据无法解析或不符合模型约束时返回。
   */
  APP_ERR_INVALID_DATA,
  /**
   * @brief 内存分配失败。
   * @details 无法为当前操作分配所需内存时返回。
   */
  APP_ERR_NO_MEMORY
} AppResult;

/**
 * @brief 用户角色。
 * @details 角色用于划分业务权限；每个服务接口仍须自行校验调用者权限。
 */
typedef enum {
  /**
   * @brief 管理员角色。
   * @details 可执行账号管理操作。
   */
  ROLE_ADMIN = 0,
  /**
   * @brief 卡用户角色。
   * @details 仅操作账号绑定的本人校园卡。
   */
  ROLE_CARD_USER = 1,
  /**
   * @brief 卡业务员角色。
   * @details 可办理开卡、查询及卡状态管理。
   */
  ROLE_CARD_CLERK = 2
} UserRole;

/**
 * @brief 校园卡状态。
 * @details `CARD_STATUS_ACTIVE` 表示允许资金业务，`CARD_STATUS_FROZEN` 表示禁止充值与消费。
 */
typedef enum {
  /**
   * @brief 正常状态。
   * @details 允许执行校园卡资金业务。
   */
  CARD_STATUS_ACTIVE = 0,
  /**
   * @brief 冻结状态。
   * @details 禁止充值与消费。
   */
  CARD_STATUS_FROZEN = 1
} CardStatus;

#endif // CAMP_CARD_COMMON_H
