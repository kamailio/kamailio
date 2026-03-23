/*
 * tls_wolfssl - PKCS#11 private key loader
 *
 * Parses a PKCS#11 URI of the form:
 *   pkcs11:module=/path/to/pkcs11.so;token=token_name;object=key_label[;pin=<pin>]
 *
 * pin= variants:
 *   pin=secret          literal PIN
 *   pin=env:VAR_NAME    read PIN from environment variable VAR_NAME
 *   pin=file:/path      read PIN from first line of file at /path
 *
 * Caches up to PKCS11_MAX_DEVS wolfSSL crypto devices.
 * Each unique (module, token) pair is registered as a separate device;
 *
 * For each crypto device the corresponding Pkcs11Token(s) are created
 * per thread and cached by index.
 */

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include <wolfssl/wolfcrypt/wc_pkcs11.h>
#include <wolfssl/wolfcrypt/cryptocb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <pthread.h>
#include <threads.h>

#include "../../core/dprint.h"

#define PKCS11_MAX_DEVS 4

typedef struct
{
	int module_id;
	char lib[128];
	Pkcs11Dev dev;
	int used;

} pkcs11_module_t;

typedef struct
{
	int token_id;
	char longname[255]; /* lib:token concat for lookup */
	char token[128];
	char pin[128];
	pkcs11_module_t *dev;
	int used;
} pkcs11_token_t;

typedef struct
{
	int token_id;
	char private_key[256];
} pkcs11_context_t;

typedef struct
{
	Pkcs11Token tok;
	int used;
} pkcs11_device_t;

static pkcs11_module_t _pkcs11_modules[PKCS11_MAX_DEVS];
static pkcs11_token_t _pkcs11_tokens[PKCS11_MAX_DEVS];

thread_local static pkcs11_device_t _pkcs11_devices[PKCS11_MAX_DEVS];
extern thread_local int thread_etask_pidx;
extern int ksr_tcp_main_threads;


/*
 * Resolve a pin= value into a NUL-terminated PIN string written into
 * out[0..out_sz-1].  Supports three forms:
 *   plain text  : pin=secret
 *   env variable: pin=env:VAR_NAME
 *   file        : pin=file:/path/to/pin-file  (first line, newline stripped)
 * Returns 1 on success, 0 on failure.
 */
static int pkcs11_resolve_pin(const char *raw, char *out, int out_sz)
{
	if(strncmp(raw, "env:", 4) == 0) {
		const char *val = getenv(raw + 4);
		if(!val) {
			LM_ERR("pkcs11: env variable '%s' not set\n", raw + 4);
			return 0;
		}
		if((int)strlen(val) >= out_sz) {
			LM_ERR("pkcs11: PIN from env '%s' too long\n", raw + 4);
			return 0;
		}
		strncpy(out, val, out_sz - 1);
		out[out_sz - 1] = '\0';
		return 1;
	}
	if(strncmp(raw, "file:", 5) == 0) {
		FILE *f = fopen(raw + 5, "r");
		if(!f) {
			LM_ERR("pkcs11: cannot open PIN file '%s'\n", raw + 5);
			return 0;
		}
		if(!fgets(out, out_sz, f)) {
			fclose(f);
			LM_ERR("pkcs11: PIN file '%s' is empty\n", raw + 5);
			return 0;
		}
		fclose(f);
		/* strip trailing newline */
		int n = (int)strlen(out);
		if(n > 0 && out[n - 1] == '\n')
			out[n - 1] = '\0';
		return 1;
	}
	/* literal PIN */
	if((int)strlen(raw) >= out_sz) {
		LM_ERR("pkcs11: literal PIN too long\n");
		return 0;
	}
	strncpy(out, raw, out_sz - 1);
	out[out_sz - 1] = '\0';
	return 1;
}


/*
 * Parse a single field=value pair from a pkcs11 URI fragment.
 * The URI fragment is the part after "pkcs11:", with components separated by ";".
 * Returns 1 if the field was found and copied into out, 0 otherwise.
 */
static int pkcs11_parse_field(
		const char *uri, const char *field, char *out, int out_sz)
{
	const char *p;
	const char *end;
	int len;

	p = strstr(uri, field);
	if(!p)
		return 0;
	p += strlen(field);
	end = strchr(p, ';');
	len = end ? (int)(end - p) : (int)strlen(p);
	if(len <= 0 || len >= out_sz)
		return 0;
	memcpy(out, p, len);
	out[len] = '\0';
	return 1;
}

/**
 * decodes a URI-encoded string.
 * @param dst: Destination buffer (must be at least as large as src)
 * @param src: The encoded string
 */
void uri_decode(char *dst, const char *src)
{
	char a, b;
	while(*src) {
		if((*src == '%') && ((a = src[1]) && (b = src[2]))
				&& (isxdigit(a) && isxdigit(b))) {

			// Convert hex to character
			if(a >= 'a')
				a -= 'a' - 'A';
			if(a >= 'A')
				a -= ('A' - 10);
			else
				a -= '0';

			if(b >= 'a')
				b -= 'a' - 'A';
			if(b >= 'A')
				b -= ('A' - 10);
			else
				b -= '0';

			*dst++ = 16 * a + b;
			src += 3;
		} else {
			*dst++ = *src++;
		}
	}
	*dst = '\0';
}


/*
 * Load a PKCS#11 private key identified by a URI into a WOLFSSL_CTX.
 *
 * URI format:
 *   pkcs11:module=<path_to_pkcs11_lib.so>;token=<token_name>;object=<key_label>
 *               [;pin=<pin>|env:<VAR>|file:<path>]
 *
 * - module: absolute path to PKCS#11 shared library (mandatory)
 * - token:  PKCS#11 token name (mandatory)
 * - object: key label (mandatory) - used to look up the private key
 * - pin:    token PIN; optional if the token requires no login
 *           pin=secret          literal PIN
 *           pin=env:VAR_NAME    read from environment variable
 *           pin=file:/path      read first line from file
 *
 * Devices are cached by (module, token): the same pair reuses the
 * already-registered devId across multiple calls.
 *
 * Returns 0 on success, -1 on failure.
 */


static int my_FilterPk(int devId, wc_CryptoInfo *info, void *ctx)
{
	int ret = CRYPTOCB_UNAVAILABLE;

	if(info->algo_type == WC_ALGO_TYPE_PK)
		ret = wc_Pkcs11_CryptoDevCb(devId, info, ctx);
	return ret;
}


static int pkcs11_init_token(Pkcs11Token *tok, pkcs11_token_t *entry)
{
	int ret;

	if(tok == NULL || entry == NULL)
		return -1;

	memset(tok, 0, sizeof(*tok));
	if(1) {
		ret = wc_Pkcs11Token_InitName(tok, &(entry->dev->dev), entry->token,
				(int)strlen(entry->token), (const unsigned char *)entry->pin,
				(int)strlen(entry->pin));
	} else {
		ret = wc_Pkcs11Token_InitName_NoLogin(
				tok, &entry->dev->dev, entry->token, (int)strlen(entry->token));
	}

	if(ret != 0) {
		LM_ERR("pkcs11: token init failed ret=%d token='%s'\n", ret,
				entry->token);
		return -1;
	}

	return 0;
}

int tls_pkcs11_open_token(WOLFSSL *ssl)
{
	WOLFSSL_CTX *ctx = wolfSSL_get_SSL_CTX(ssl);
	pkcs11_context_t *ctxd = wolfSSL_CTX_get_ex_data(ctx, 0);

	if(ctxd == NULL)
		return 0;

	int idx = ctxd->token_id;
	int devId = 16
						* (ksr_tcp_main_threads > 0 ? thread_etask_pidx + 1
													: process_no + 1)
				+ idx;

	if(!likely(_pkcs11_devices[idx].used)) {
		Pkcs11Token *token = &_pkcs11_devices[idx].tok;

		pkcs11_init_token(token, _pkcs11_tokens + idx);
		_pkcs11_devices[idx].used = devId;

		if(wc_CryptoCb_RegisterDevice(devId, my_FilterPk, token) != 0) {
			LM_ERR("pkcs11: failed to register runtime devId=%d token='%s'\n",
					devId, _pkcs11_tokens[idx].token);
			return -1;
		}

		wc_Pkcs11Token_Open(token, 0);
		LM_INFO("Created token:%d for thread:%d\n: devId=%d", idx,
				thread_etask_pidx, devId);
	} else {
		if(likely(_pkcs11_devices[idx].used != devId)) {
			LM_WARN("pkcs11: device used=%d mismatch (expected devId=%d)\n",
					_pkcs11_devices[idx].used, devId);
			return -1;
		}
	}

	wolfSSL_use_PrivateKey_Label(ssl, ctxd->private_key, devId);

	return 0;
}

int tls_pkcs11_set_key(WOLFSSL_CTX *ctx, char *key)
{
	char uri[1024];
	char token_name[256] = "";
	char obj_name[256] = "";
	char lib_path[512] = "";
	char pin_raw[256] = "";
	char pin_buf[256] = "";

	int i;

	Pkcs11Token *temp_tok;

	if(!ctx || !key) {
		LM_ERR("tls_pkcs11_set_key: NULL argument\n");
		return -1;
	}
	if(strncmp(key, "pkcs11:", 7) != 0) {
		LM_ERR("tls_pkcs11_set_key: not a pkcs11 URI: '%s'\n", key);
		return -1;
	}
	uri_decode(uri, key + 7);

	pkcs11_parse_field(uri, "module=", lib_path, sizeof(lib_path));
	pkcs11_parse_field(uri, "token=", token_name, sizeof(token_name));
	pkcs11_parse_field(uri, "object=", obj_name, sizeof(obj_name));
	if(pkcs11_parse_field(uri, "pin=", pin_raw, sizeof(pin_raw))) {
		if(!pkcs11_resolve_pin(pin_raw, pin_buf, sizeof(pin_buf)))
			return -1;
	}

	if(!lib_path[0]) {
		LM_ERR("tls_pkcs11_set_key: missing 'module' in URI: '%s'\n", key);
		return -1;
	}
	if(!token_name[0]) {
		LM_ERR("tls_pkcs11_set_key: missing 'token' in URI: '%s'\n", key);
		return -1;
	}
	if(!obj_name[0]) {
		LM_ERR("tls_pkcs11_set_key: missing 'object' in URI: '%s'\n", key);
		return -1;
	}

	/* Search the cache for an already-initialised (lib, token) pair */
	pkcs11_token_t *cur_token = NULL;
	int size = snprintf(NULL, 0, "%s:%s", lib_path, token_name);
	char *lookup_name = malloc(size + 1);
	int token_slot;

	snprintf(lookup_name, size + 1, "%s:%s", lib_path, token_name);
	for(i = 0; i < PKCS11_MAX_DEVS; i++) {
		if(!_pkcs11_tokens[i].used)
			break;
		if(strcmp(_pkcs11_tokens[i].longname, lookup_name) == 0) {
			cur_token = &(_pkcs11_tokens[i]);
			break;
		}
	}
	token_slot = i;

	if(!cur_token) {
		LM_INFO("Creating a new token: key = %s, slot = %d\n", lookup_name,
				token_slot);
		if(token_slot == PKCS11_MAX_DEVS) {
			LM_ERR("tls_pkcs11_set_key: too many tokens registered\n");
			return -1;
		}

		cur_token = &(_pkcs11_tokens[token_slot]);

		pkcs11_module_t *cur_module = NULL;
		for(i = 0; i < PKCS11_MAX_DEVS; i++) {
			if(!_pkcs11_modules[i].used)
				break;
			if(strcmp(_pkcs11_modules[i].lib, lib_path) == 0) {
				cur_module = &(_pkcs11_modules[i]);
				break;
			}
		}

		if(cur_module == NULL) {
			if(i == PKCS11_MAX_DEVS) {
				LM_ERR("tls_pkcs11_set_key: too many modules registered\n");
				return -1;
			}

			cur_module = &(_pkcs11_modules[i]);
			cur_module->used = 1;
			cur_module->module_id = i;
			int rc = wc_Pkcs11_Initialize(&(cur_module->dev), lib_path, NULL);
			LM_INFO("Creating a new device: key = %s, slot = %d\n", lib_path,
					i);
			if(rc != 0) {
				LM_ERR("tls_pkcs11_set_key: could not register  modules: %s\n",
						lib_path);
			}
		}

		cur_token->used = 1;
		cur_token->token_id = token_slot;

		strncpy(cur_token->longname, lookup_name, 254);
		cur_token->longname[254] = '\0';

		strncpy(cur_token->token, token_name, 127);
		cur_token->token[127] = '\0';

		strncpy(cur_token->pin, pin_buf, 127);
		cur_token->token[127] = '\0';

		cur_token->dev = cur_module;
	}


	pkcs11_context_t *ctxd = wolfSSL_CTX_get_ex_data(ctx, 0);
	int devId = cur_token->token_id;

	if(ctxd == NULL) {
		ctxd = (pkcs11_context_t *)malloc(sizeof(pkcs11_context_t));
		if(ctxd == NULL) {
			LM_ERR("tls_pkcs11_set_key: no more pkg memory for ctx metadata\n");
			return -1;
		}
		memset(ctxd, 0, sizeof(*ctxd));
		wolfSSL_CTX_set_ex_data(ctx, 0, ctxd);
	}
	ctxd->token_id = cur_token->token_id;
	;
	strncpy(ctxd->private_key, obj_name, sizeof(ctxd->private_key) - 1);
	ctxd->private_key[sizeof(ctxd->private_key) - 1] = '\0';

	temp_tok = &_pkcs11_devices[cur_token->token_id].tok;
	pkcs11_init_token(temp_tok, cur_token);
	_pkcs11_devices[cur_token->token_id].used = 1;

	if(wc_CryptoCb_RegisterDevice(devId, my_FilterPk, temp_tok) != 0) {
		LM_ERR("pkcs11: failed to register runtime devId=%d token='%s'\n", 0,
				token_name);
		wc_Pkcs11Token_Final(temp_tok);
		return -1;
	}


	/* Create a temporary SSL object for the cert/key check.
	 * Must not place any token in the SSL context as this causes run time
	 * errors when the thread tries to use its specific token.
	*/
	WOLFSSL *ssl = wolfSSL_new(ctx);
	/* Bind the crypto device to this SSL context */
	if(wolfSSL_SetDevId(ssl, devId) != WOLFSSL_SUCCESS) {
		LM_ERR("tls_pkcs11_set_key: wolfSSL_SetDevId failed"
			   " devId=%d\n",
				devId);
		return -1;
	}

	/* Register the private key by its PKCS#11 label */
	if(wolfSSL_use_PrivateKey_Label(ssl, obj_name, devId) != WOLFSSL_SUCCESS) {
		LM_ERR("tls_pkcs11_set_key: wolfSSL_use_PrivateKey_Label"
			   " failed label='%s' devId=%d\n",
				obj_name, devId);
		return -1;
	}
	if(wolfSSL_check_private_key(ssl) != WOLFSSL_SUCCESS) {
		LM_ERR("tls_pkcs11_set_key: wolfSSL_check_private_key"
			   " failed label='%s' devId=%d\n",
				obj_name, devId);
		return -1;
	}

	wolfSSL_free(ssl);

	LM_INFO("tls_pkcs11_set_key[%p]: key loaded token='%s' object='%s'"
			" devId=%d : private key matches cert!\n",
			ctx, token_name, obj_name, devId);


	return 0;
}
