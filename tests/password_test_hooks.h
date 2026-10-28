#ifndef CAMP_CARD_PASSWORD_TEST_HOOKS_H
#define CAMP_CARD_PASSWORD_TEST_HOOKS_H

#include <openssl/evp.h>

// 仅供故障测试链接，签名须与被替换的 OpenSSL 函数一致。

int password_test_rand_bytes(unsigned char *buffer, int count);
int password_test_pbkdf2(const char *, int, const unsigned char *, int, int,
                         const EVP_MD *, int, unsigned char *);

#endif // CAMP_CARD_PASSWORD_TEST_HOOKS_H
