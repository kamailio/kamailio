#include "cdp_tls.h"

cfg_option_t methods[] = {{"TLSv1", .val = TLS_USE_TLSv1},
		{"TLSv1.0", .val = TLS_USE_TLSv1},
		{"TLSv1+", .val = TLS_USE_TLSv1_PLUS},
		{"TLSv1.0+", .val = TLS_USE_TLSv1_PLUS},
		{"TLSv1.1", .val = TLS_USE_TLSv1_1},
		{"TLSv1.1+", .val = TLS_USE_TLSv1_1_PLUS},
		{"TLSv1.2", .val = TLS_USE_TLSv1_2},
		{"TLSv1.2+", .val = TLS_USE_TLSv1_2_PLUS},
		{"TLSv1.3", .val = TLS_USE_TLSv1_3},
		{"TLSv1.3+", .val = TLS_USE_TLSv1_3_PLUS}, {0}};

tls_methods_t tls_methods[TLS_METHOD_MAX];

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
void init_ssl_methods(void)
{
	/* openssl 1.1.0+ */
	memset(tls_methods, 0, sizeof(tls_methods));

	tls_methods[TLS_USE_TLSv1_cli - 1].TLSMethod = TLS_client_method();
	tls_methods[TLS_USE_TLSv1_cli - 1].TLSMethodMin = TLS1_VERSION;
	tls_methods[TLS_USE_TLSv1_cli - 1].TLSMethodMax = TLS1_VERSION;
	tls_methods[TLS_USE_TLSv1_srv - 1].TLSMethod = TLS_server_method();
	tls_methods[TLS_USE_TLSv1_srv - 1].TLSMethodMin = TLS1_VERSION;
	tls_methods[TLS_USE_TLSv1_srv - 1].TLSMethodMax = TLS1_VERSION;
	tls_methods[TLS_USE_TLSv1 - 1].TLSMethod = TLS_method();
	tls_methods[TLS_USE_TLSv1 - 1].TLSMethodMin = TLS1_VERSION;
	tls_methods[TLS_USE_TLSv1 - 1].TLSMethodMax = TLS1_VERSION;

	tls_methods[TLS_USE_TLSv1_1_cli - 1].TLSMethod = TLS_client_method();
	tls_methods[TLS_USE_TLSv1_1_cli - 1].TLSMethodMin = TLS1_1_VERSION;
	tls_methods[TLS_USE_TLSv1_1_cli - 1].TLSMethodMax = TLS1_1_VERSION;
	tls_methods[TLS_USE_TLSv1_1_srv - 1].TLSMethod = TLS_server_method();
	tls_methods[TLS_USE_TLSv1_1_srv - 1].TLSMethodMin = TLS1_1_VERSION;
	tls_methods[TLS_USE_TLSv1_1_srv - 1].TLSMethodMax = TLS1_1_VERSION;
	tls_methods[TLS_USE_TLSv1_1 - 1].TLSMethod = TLS_method();
	tls_methods[TLS_USE_TLSv1_1 - 1].TLSMethodMin = TLS1_1_VERSION;
	tls_methods[TLS_USE_TLSv1_1 - 1].TLSMethodMax = TLS1_1_VERSION;

	tls_methods[TLS_USE_TLSv1_2_cli - 1].TLSMethod = TLS_client_method();
	tls_methods[TLS_USE_TLSv1_2_cli - 1].TLSMethodMin = TLS1_2_VERSION;
	tls_methods[TLS_USE_TLSv1_2_cli - 1].TLSMethodMax = TLS1_2_VERSION;
	tls_methods[TLS_USE_TLSv1_2_srv - 1].TLSMethod = TLS_server_method();
	tls_methods[TLS_USE_TLSv1_2_srv - 1].TLSMethodMin = TLS1_2_VERSION;
	tls_methods[TLS_USE_TLSv1_2_srv - 1].TLSMethodMax = TLS1_2_VERSION;
	tls_methods[TLS_USE_TLSv1_2 - 1].TLSMethod = TLS_method();
	tls_methods[TLS_USE_TLSv1_2 - 1].TLSMethodMin = TLS1_2_VERSION;
	tls_methods[TLS_USE_TLSv1_2 - 1].TLSMethodMax = TLS1_2_VERSION;

#if OPENSSL_VERSION_NUMBER >= 0x1010100fL && !defined(LIBRESSL_VERSION_NUMBER)
	tls_methods[TLS_USE_TLSv1_3_cli - 1].TLSMethod = TLS_client_method();
	tls_methods[TLS_USE_TLSv1_3_cli - 1].TLSMethodMin = TLS1_3_VERSION;
	tls_methods[TLS_USE_TLSv1_3_cli - 1].TLSMethodMax = TLS1_3_VERSION;
	tls_methods[TLS_USE_TLSv1_3_srv - 1].TLSMethod = TLS_server_method();
	tls_methods[TLS_USE_TLSv1_3_srv - 1].TLSMethodMin = TLS1_3_VERSION;
	tls_methods[TLS_USE_TLSv1_3_srv - 1].TLSMethodMax = TLS1_3_VERSION;
	tls_methods[TLS_USE_TLSv1_3 - 1].TLSMethod = TLS_method();
	tls_methods[TLS_USE_TLSv1_3 - 1].TLSMethodMin = TLS1_3_VERSION;
	tls_methods[TLS_USE_TLSv1_3 - 1].TLSMethodMax = TLS1_3_VERSION;
#endif

	/* ranges of TLS versions (require a minimum TLS version) */
	tls_methods[TLS_USE_TLSv1_PLUS - 1].TLSMethod = TLS_method();
	tls_methods[TLS_USE_TLSv1_PLUS - 1].TLSMethodMin = TLS1_VERSION;

	tls_methods[TLS_USE_TLSv1_1_PLUS - 1].TLSMethod = TLS_method();
	tls_methods[TLS_USE_TLSv1_1_PLUS - 1].TLSMethodMin = TLS1_1_VERSION;

	tls_methods[TLS_USE_TLSv1_2_PLUS - 1].TLSMethod = TLS_method();
	tls_methods[TLS_USE_TLSv1_2_PLUS - 1].TLSMethodMin = TLS1_2_VERSION;

#if OPENSSL_VERSION_NUMBER >= 0x1010100fL && !defined(LIBRESSL_VERSION_NUMBER)
	tls_methods[TLS_USE_TLSv1_3_PLUS - 1].TLSMethod = TLS_method();
	tls_methods[TLS_USE_TLSv1_3_PLUS - 1].TLSMethodMin = TLS1_3_VERSION;
#endif
}
#endif

/*
 * Convert TLS method string to integer
 */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
int tls_parse_method(str *m)
{
	cfg_option_t *opt;

	if(!m) {
		LM_BUG("Invalid parameter value\n");
		return -1;
	}

	opt = cfg_lookup_token(methods, m);
	if(!opt)
		return -1;

	return opt->val;
}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
SSL_CTX *init_ssl_ctx(int method)
{
	SSL_CTX *ctx;

	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	/* libssl >= 1.1.0 */
	ctx = SSL_CTX_new(tls_methods[method - 1].TLSMethod);
	if(ctx == NULL) {
		unsigned long e = 0;
		e = ERR_peek_last_error();
		LM_ERR("Failed to create SSL context (%lu: %s / %s)\n", e,
				ERR_error_string(e, NULL), ERR_reason_error_string(e));
		return NULL;
	}

	if(method > TLS_USE_TLSvRANGE) {
		if(tls_methods[method - 1].TLSMethodMin) {
			SSL_CTX_set_min_proto_version(
					ctx, tls_methods[method - 1].TLSMethodMin);
		}
	} else {
		if(tls_methods[method - 1].TLSMethodMin) {
			SSL_CTX_set_min_proto_version(
					ctx, tls_methods[method - 1].TLSMethodMin);
		}
		if(tls_methods[method - 1].TLSMethodMax) {
			SSL_CTX_set_max_proto_version(
					ctx, tls_methods[method - 1].TLSMethodMax);
		}
	}
	return ctx;
}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
int load_certificates(SSL_CTX *ctx, str *cert, str *key)
{
	str cert_fixed = STR_NULL;
	str key_fixed = STR_NULL;

	if(pkg_str_dup(&cert_fixed, cert) < 0) {
		LM_ERR("Failed to copy cert parameter\n");
		return -1;
	}
	if(pkg_str_dup(&key_fixed, key) < 0) {
		LM_ERR("Failed to copy key parameter\n");
		pkg_free(cert_fixed.s);
		return -1;
	}
	if(!SSL_CTX_use_certificate_chain_file(ctx, cert_fixed.s)) {
		LM_ERR("Unable to load certificate file\n");
		TLS_LM_ERR("load_cert:", ctx);
		pkg_free(key_fixed.s);
		pkg_free(cert_fixed.s);
		return -1;
	}
	if(SSL_CTX_use_PrivateKey_file(ctx, key_fixed.s, SSL_FILETYPE_PEM) <= 0) {
		LM_ERR("Unable to load private key file\n");
		TLS_LM_ERR("load_private_key:", ctx);
		pkg_free(key_fixed.s);
		return -1;
	}
	if(!SSL_CTX_check_private_key(ctx)) {
		LM_ERR("Private key does not match the public "
			   "certificate\n");
		TLS_LM_ERR("load_private_key:", ctx);
		return -1;
	}
	return 0;
}
#endif

/*
 * Get any leftover errors from OpenSSL and print them.
 * ERR_get_error() also removes the error from the OpenSSL error stack.
 * This is useful to call before any SSL_* IO calls to make sure
 * we don't have any leftover errors from previous calls (OpenSSL docs).
 */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
void cdp_openssl_clear_errors(void)
{
	int i;
	char err[256];
	while((i = ERR_get_error())) {
		ERR_error_string(i, err);
		LM_INFO("clearing leftover error before SSL_* calls: %s\n", err);
	}
}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
SSL *init_ssl_conn(int client_fd, SSL_CTX *ctx)
{
	X509 *cert = NULL;
	SSL *ssl = NULL;
	int ssl_ret, error;

	/* Create an SSL object */
	ssl = SSL_new(ctx);
	if(!ssl) {
		LM_ERR("Failed to create SSL object\n");
		TLS_LM_ERR("create_openssl_struct:", ctx);
		goto cleanup;
	}
	/* Set up the TLS connection */
	if(SSL_set_fd(ssl, client_fd) != 1) {
		LM_ERR("Failed to set up TLS connection\n");
		TLS_LM_ERR("set_fd:", ctx);
		goto cleanup;
	}
	/* Perform the TLS handshake */
	cdp_openssl_clear_errors();
	ssl_ret = SSL_connect(ssl);
	if(ssl_ret != 1) {
		error = SSL_get_error(ssl, ssl_ret);
		LM_ERR("TLS handshake failed with error:%s\n",
				ERR_error_string(error, 0));
		goto cleanup;
	}
	if(ca_list.s) {
		cert = SSL_get_peer_certificate(ssl);
		if(!cert) {
			LM_ERR("No server certificate received\n");
			goto cleanup;
		}
		LM_INFO("Server certificate received\n");

		/* Verify the server certificate */
		long verify_result = SSL_get_verify_result(ssl);
		if(verify_result != X509_V_OK) {
			LM_ERR("Server certificate verification failed: %s\n",
					X509_verify_cert_error_string(verify_result));
			goto cleanup;
		}
	}

	return ssl;

cleanup:
	if(cert)
		X509_free(cert);
	if(ssl)
		SSL_free(ssl);
	if(ctx)
		SSL_CTX_free(ctx);

	return NULL;
}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
void cleanup_ssl(SSL_CTX *tls_ctx, SSL *tls_conn)
{
	SSL_shutdown(tls_conn);
	SSL_free(tls_conn);
	SSL_CTX_free(tls_ctx);
}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
int to_ssl(SSL_CTX **tls_ctx_p, SSL **tls_conn_p, int tcp_sock, int method)
{
	*tls_ctx_p = init_ssl_ctx(method);
	if(!(*tls_ctx_p)) {
		LM_ERR("Error on initialising TLS context\n");
		return -1;
	}
	if(certificate.s && private_key.s) {
		if(load_certificates(*tls_ctx_p, &certificate, &private_key) < 0) {
			LM_ERR("Error on loading certificates\n");
			return -1;
		}
	}
	if(ca_list.s) {
		if(SSL_CTX_load_verify_locations(*tls_ctx_p, ca_list.s, NULL) != 1) {
			LM_ERR("Unable to load CA list file '%s'\n",
					(ca_list.s) ? ca_list.s : "");
			TLS_LM_ERR("load_ca_list:", *tls_ctx_p);
			return -1;
		}
	}
	*tls_conn_p = init_ssl_conn(tcp_sock, *tls_ctx_p);
	if(!(*tls_conn_p)) {
		LM_ERR("Error on negociating TLS connection\n");
		return -1;
	}
	return 0;
}
#endif
