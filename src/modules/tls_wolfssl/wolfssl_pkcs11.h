/*
 * tls_wolfssl - PKCS#11 private key loader
 *
 * URI format:
 *   pkcs11:module=/path/to/pkcs11.so;token=<name>;object=<label>[;pin=<pin>]
 *
 * pin= variants:
 *   pin=secret          literal PIN
 *   pin=env:VAR_NAME    read from environment variable
 *   pin=file:/path      read first line from file
 */

#ifndef WOLFSSL_PKCS11_H
#define WOLFSSL_PKCS11_H

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

/*
 * Load a PKCS#11 private key into ctx.
 * key format: pkcs11:token=<name>;slot=<name>;object=<label>[;module-path=<lib>]
 * Returns 1 on success, 0 on failure.
 */
int tls_pkcs11_set_key(WOLFSSL_CTX *ctx, char *key, int check_key);

typedef struct
{
	int token_id;
	char config_key[256];
	char private_key[256];
	int *index;
} pkcs11_context_t;
#endif /* WOLFSSL_PKCS11_H */
