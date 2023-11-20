#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <stdio.h>

#define HANDLE_ERROR                 \
    {                                \
        perror("bank error\n");      \
        ERR_print_errors_fp(stderr); \
        return -1;                   \
    }
;

ssize_t rsa_encrypt(EVP_PKEY *key, unsigned char *data, size_t data_len, unsigned char **out) {
    size_t out_len;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);

    if (!ctx) HANDLE_ERROR;
    if (EVP_PKEY_encrypt_init(ctx) <= 0) HANDLE_ERROR;
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
        HANDLE_ERROR;

    if (EVP_PKEY_encrypt(ctx, NULL, &out_len, data, data_len) <= 0)
        HANDLE_ERROR;

    *out = OPENSSL_malloc(out_len);
    if (!*out)
        HANDLE_ERROR;

    if (EVP_PKEY_encrypt(ctx, *out, &out_len, data, data_len) <= 0)
        HANDLE_ERROR;

    return out_len;
}

ssize_t rsa_decrypt(EVP_PKEY *key, unsigned char *data, size_t data_len, unsigned char **out) {
    
    size_t out_len;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);

    if (!ctx) HANDLE_ERROR;
    if (EVP_PKEY_decrypt_init(ctx) <= 0) HANDLE_ERROR;
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
        HANDLE_ERROR;
    if (EVP_PKEY_decrypt(ctx, NULL, &out_len, data, data_len) <= 0)
        HANDLE_ERROR;
    if (EVP_PKEY_decrypt(ctx, data, &out_len, data, data_len) <= 0)
        HANDLE_ERROR;

    return out_len;
}

EVP_PKEY *rsa_readkey(const char *filename) {
    EVP_PKEY *key;
    FILE *key_file;

    key_file = fopen(filename, "r");
    if (!key_file) return NULL;

    key = PEM_read_PrivateKey(key_file, NULL, NULL, NULL);
    if (!key) return NULL;

    fclose(key_file);

    return key;
}