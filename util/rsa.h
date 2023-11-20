#ifndef __RSA_H__
#define __RSA_H__

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#define HANDLE_ERROR                 \
    {                                \
        perror("bank error\n");      \
        ERR_print_errors_fp(stderr); \
        return -1;                   \
    }
;

ssize_t rsa_encrypt(EVP_PKEY *key, unsigned char *data, size_t data_len, unsigned char **out);
ssize_t rsa_decrypt(EVP_PKEY *key, unsigned char *data, size_t data_len);
EVP_PKEY *rsa_readkey(const char *filename);

#endif