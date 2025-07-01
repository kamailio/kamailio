#ifndef __CDP_TLS_H
#define __CDP_TLS_H

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../../core/dprint.h"
#include "../../core/cfg_parser.h"
#include "../../core/ut.h"

/**
 * Available TLS methods
 */
enum tls_method
{
	TLS_METHOD_UNSPEC = 0,
	TLS_USE_TLSv1_cli,
	TLS_USE_TLSv1_srv,
	TLS_USE_TLSv1, /* only TLSv1.0 */
	TLS_USE_TLSv1_1_cli,
	TLS_USE_TLSv1_1_srv,
	TLS_USE_TLSv1_1, /* only TLSv1.1 */
	TLS_USE_TLSv1_2_cli,
	TLS_USE_TLSv1_2_srv,
	TLS_USE_TLSv1_2, /* only TLSv1.2 */
	TLS_USE_TLSv1_3_cli,
	TLS_USE_TLSv1_3_srv,
	TLS_USE_TLSv1_3,	  /* only TLSv1.3 */
	TLS_USE_TLSvRANGE,	  /* placeholder - TLSvX ranges must be after it */
	TLS_USE_TLSv1_PLUS,	  /* TLSv1.0 or greater */
	TLS_USE_TLSv1_1_PLUS, /* TLSv1.1 or greater */
	TLS_USE_TLSv1_2_PLUS, /* TLSv1.2 or greater */
	TLS_USE_TLSv1_3_PLUS, /* TLSv1.3 or greater */
	TLS_METHOD_MAX
};

typedef struct tls_methods_s
{
	const SSL_METHOD *TLSMethod;
	int TLSMethodMin;
	int TLSMethodMax;
} tls_methods_t;

extern cfg_option_t methods[];
extern tls_methods_t tls_methods[];
extern str private_key;
extern str certificate;
extern str ca_list;

static inline int tls_err_ret(char *s, SSL_CTX *ctx)
{
	long err;
	int ret = 0;

	if(ctx) {
		while((err = ERR_get_error())) {
			ret = 1;
			LM_ERR("%s%s\n", s ? s : "", ERR_error_string(err, 0));
		}
	}
	return ret;
}

#define TLS_LM_ERR(s, ctx)     \
	do {                       \
		tls_err_ret((s), ctx); \
	} while(0)

int tls_parse_method(str *method);
void cdp_openssl_clear_errors(void);
void init_ssl_methods(void);
SSL_CTX *init_ssl_ctx(int method);
SSL *init_ssl_conn(int client_fd, SSL_CTX *ctx);
int to_ssl(SSL_CTX **tls_ctx_p, SSL **tls_conn_p, int tcp_sock, int method);
void cleanup_ssl(SSL_CTX *tls_ctx, SSL *tls_conn);

#endif
