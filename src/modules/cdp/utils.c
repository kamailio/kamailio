#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../../core/dprint.h"
#include "utils.h"

/*
 * Get any leftover errors from OpenSSL and print them.
 * ERR_get_error() also removes the error from the OpenSSL error stack.
 * This is useful to call before any SSL_* IO calls to make sure
 * we don't have any leftover errors from previous calls (OpenSSL docs).
 */
void cdp_openssl_clear_errors(void)
{
	int i;
	char err[256];
	while((i = ERR_get_error())) {
		ERR_error_string(i, err);
		LM_INFO("clearing leftover error before SSL_* calls: %s", err);
	}
}