/**
 * Copyright (C) 2021 kamailio.org
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <stir_shaken.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/data_lump.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"

MODULE_VERSION

// Authentication service
static str stirshaken_as_default_key = str_init("");

// Verification service
static int stirshaken_vs_verify_x509_cert_path = 1;
static str stirshaken_vs_ca_dir = str_init("");
static str stirshaken_vs_crl_dir = str_init("");
static int stirshaken_vs_identity_expire_s = 60;
static int stirshaken_vs_connect_timeout_s = 5;

static int stirshaken_vs_cache_certificates = 0;
static size_t stirshaken_vs_cache_expire_s = 120;
static str stirshaken_vs_cache_dir = str_init("");

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_stirshaken_check_identity(sip_msg_t *msg, char *str1, char *str2);
static int w_stirshaken_check_identity_with_cert(sip_msg_t *msg, char *cert_path, char *str2);
static int w_stirshaken_check_identity_with_key(sip_msg_t *msg, char *pkey_path, char *str2);
static int w_stirshaken_add_identity(sip_msg_t *msg, str *px5u, str *pattest, str *porigtn_val, str *pdesttn_val, str *porigid);
static int w_stirshaken_add_identity_with_key(sip_msg_t *msg, str *px5u, str *pattest, str *porigtn_val, str *pdesttn_val, str *porigid, str *pkeypath);


/* clang-format off */
static cmd_export_t cmds[]={
	{"stirshaken_check_identity", (cmd_function)w_stirshaken_check_identity, 0,
		0, 0, ANY_ROUTE},
	{"stirshaken_check_identity_with_cert", (cmd_function)w_stirshaken_check_identity_with_cert, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"stirshaken_check_identity_with_key", (cmd_function)w_stirshaken_check_identity_with_key, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"stirshaken_add_identity", (cmd_function)w_stirshaken_add_identity, 5,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{"stirshaken_add_identity_with_key", (cmd_function)w_stirshaken_add_identity_with_key, 6,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {

	// Authentication service
	{"as_default_key",			PARAM_STR,   &stirshaken_as_default_key},

	// Verification service
	{"vs_verify_x509_cert_path",PARAM_INT,   &stirshaken_vs_verify_x509_cert_path},
	{"vs_ca_dir",				PARAM_STR,   &stirshaken_vs_ca_dir},
	{"vs_crl_dir",				PARAM_STR,   &stirshaken_vs_crl_dir},
	{"vs_identity_expire_s",	PARAM_INT,   &stirshaken_vs_identity_expire_s},
	{"vs_connect_timeout_s",	PARAM_INT,   &stirshaken_vs_connect_timeout_s},
	{"vs_cache_certificates",	PARAM_INT,   &stirshaken_vs_cache_certificates},
	{"vs_cache_expire_s",		PARAM_INT,   &stirshaken_vs_cache_expire_s},
	{"vs_cache_dir",			PARAM_STR,   &stirshaken_vs_cache_dir},
	{0, 0, 0}
};

struct module_exports exports = {
	"stirshaken",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,              /* exported RPC methods */
	0,              /* exported pseudo-variables */
	0,              /* response function */
	mod_init,       /* module initialization function */
	child_init,     /* per child init function */
	mod_destroy    	/* destroy function */
};
/* clang-format on */

static void stirshaken_print_error_details(void *context)
{
	const char				*error_description = NULL;
	stir_shaken_error_t		error_code = STIR_SHAKEN_ERROR_GENERAL;

	if (stir_shaken_is_error_set(context)) {
		error_description = stir_shaken_get_error(context, &error_code);
		LM_DBG("failure details:\n");
		LM_DBG("failure reason is: %s\n", error_description);
		LM_DBG("failure error code is: %d\n", error_code);
	}
}

static unsigned long hash_to_long(const char *str)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;

	return hash;
}

static void hash_to_string(const char *url, char *buf, int buf_len)
{
	unsigned long hash_long = hash_to_long(url);
	snprintf(buf, buf_len, "%lu.pem", hash_long);
}

static int get_cert_name_hashed(const char *name, char *buf, int buf_len)
{
	char cert_hash[64] = { 0 };

	hash_to_string(name, cert_hash, sizeof(cert_hash));

	if (!stir_shaken_make_complete_path(buf, buf_len, stirshaken_vs_cache_dir.s, cert_hash, "/")) {
		LM_ERR("Cannot create cert name hashed\n");
		return -1;
	}

	return 0;
}

static stir_shaken_status_t shaken_callback(stir_shaken_callback_arg_t *arg)
{
	stir_shaken_context_t ss = { 0 };
	stir_shaken_cert_t cache_copy = { 0 };
	char cert_full_path[STIR_SHAKEN_BUFLEN] = { 0 };

	switch (arg->action) {

		case STIR_SHAKEN_CALLBACK_ACTION_CERT_FETCH_ENQUIRY:

			// Default behaviour for certificate fetch enquiry is to request downloading, but in some cases it would be useful to avoid that and use pre-cached certificate.
			// Here, we supply libstirshaken with certificate we cached earlier, avoiding HTTP(S) download.
			// We must return STIR_SHAKEN_STATUS_HANDLED to signal this to the library, otherwise it would execute HTTP(S) download

			if (!stirshaken_vs_cache_certificates) {
				LM_DBG("Certificate caching is turned off - requesting certificate %s to be downloaded...\n", arg->cert.public_url);
				return STIR_SHAKEN_STATUS_NOT_HANDLED;
			}

			if (-1 == get_cert_name_hashed(arg->cert.public_url, cert_full_path, STIR_SHAKEN_BUFLEN)) {
				LM_ERR("Cannot get cert name hashed\n");
				goto exit;
			}

			LM_DBG("Checking for certificate %s in cache (looking for name: %s)\n", arg->cert.public_url, cert_full_path);

			if (STIR_SHAKEN_STATUS_OK == stir_shaken_file_exists(cert_full_path)) {

				LM_DBG("Certificate %s found in cache\n", arg->cert.public_url);

				if (stirshaken_vs_cache_expire_s) {

					struct stat attr = { 0 };
					time_t now_s = time(NULL), diff = 0;

					LM_DBG("Checking cached certificate against expiration setting of %zus\n", stirshaken_vs_cache_expire_s);

					if (-1 == stat(cert_full_path, &attr)) {
						LM_ERR("Cannot get modification timestamp on certificate %s. Error code is %d (%s)\n", cert_full_path, errno, strerror(errno));
						goto exit;
					}

					if (now_s < attr.st_mtime) {
						LM_ERR("Modification timestamp on certificate %s is invalid\n", cert_full_path);
						goto exit;
					}

					diff = now_s - attr.st_mtime;

					LM_DBG("Checking cached certificate against expiration setting of %zus (now is: %lu, file modification timestamp is: %lu, difference is: %lu)\n",
						stirshaken_vs_cache_expire_s, now_s, attr.st_mtime, diff);

					if (diff > stirshaken_vs_cache_expire_s) {
						LM_NOTICE("Cached certificate %s is behind expiration threshold (%lu > %zu). Need to download new certificate...\n", cert_full_path, diff, stirshaken_vs_cache_expire_s);
						goto exit;
					} else {
						LM_NOTICE("Cached certificate %s is valid for next %lus\n", cert_full_path, stirshaken_vs_cache_expire_s - diff);
					}
				}
#ifdef STIR_SHAKEN_CAN_RW_X509_FULLCHAIN
				if (STIR_SHAKEN_STATUS_OK != stir_shaken_load_x509_from_file_fullchain(&ss, &cache_copy, cert_full_path)) {
#else
				if (!(cache_copy.x = stir_shaken_load_x509_from_file(&ss, cert_full_path))) {
#endif
					LM_ERR("Cannot load X509 from file %s\n", cert_full_path);
					goto exit;
				}

				if (STIR_SHAKEN_STATUS_OK != stir_shaken_cert_copy(&ss, &arg->cert, &cache_copy)) {
					LM_ERR("Cannot copy certificate %s\n", cert_full_path);
					stir_shaken_cert_deinit(&cache_copy);
					goto exit;
				}

				stir_shaken_cert_deinit(&cache_copy);

				return STIR_SHAKEN_STATUS_HANDLED;
			}

		default:
			LM_DBG("Certificate %s not found in cache\n", arg->cert.public_url);
			return STIR_SHAKEN_STATUS_NOT_HANDLED;
	}

exit:

	return STIR_SHAKEN_STATUS_NOT_HANDLED;
}

stir_shaken_as_t *as = NULL;
stir_shaken_vs_t *vs = NULL;

static int start_as(stir_shaken_context_t *ss)
{
	as = stir_shaken_as_create(ss);
	if (!as) {
		LM_ERR("Cannot create Authentication Service\n");
		stirshaken_print_error_details(ss);
		return -1;
	}

	if (stirshaken_as_default_key.len > 0) {

		if (STIR_SHAKEN_STATUS_OK != stir_shaken_as_load_private_key(ss, as, stirshaken_as_default_key.s)) {
			LM_ERR("Failed to load private key (%s). Please check @as_default_key param\n", stirshaken_as_default_key.s);
			stirshaken_print_error_details(ss);
			return -1;
		}
	}

	return 0;
}

static int start_vs(stir_shaken_context_t *ss)
{
	vs = stir_shaken_vs_create(ss);
	if (!vs) {
		LM_ERR("Cannot create Verification Service\n");
		stirshaken_print_error_details(ss);
		return -1;
	}

	// Handle settings

	stir_shaken_vs_set_connect_timeout(ss, vs, stirshaken_vs_connect_timeout_s);
	stir_shaken_vs_set_callback(ss, vs, shaken_callback);

	if (stirshaken_vs_cache_certificates) {

		if (!stirshaken_vs_cache_dir.len) {
			LM_ERR("Certificate caching is turned on but cache dir is not set. Please set cache dir\n");
			return -1;
		}

		if (STIR_SHAKEN_STATUS_OK != stir_shaken_dir_exists(stirshaken_vs_cache_dir.s)) {
			LM_ERR("Certificate caching is turned on but cache dir %s does not exist. Please check cache dir name\n", stirshaken_vs_cache_dir.s);
			return -1;
		}
	}

	if (stirshaken_vs_verify_x509_cert_path) {

		stir_shaken_vs_set_x509_cert_path_check(ss, vs, 1);

		if (stirshaken_vs_ca_dir.len > 0) {
			if (STIR_SHAKEN_STATUS_OK != stir_shaken_vs_load_ca_dir(ss, vs, stirshaken_vs_ca_dir.s)) {
				LM_ERR("Failed to init X509 cert store with CA dir\n");
				stirshaken_print_error_details(ss);
				return -1;
			}
		} else {
			LM_WARN("Cert path check is turned on, but CA dir is not set. No end entity certificate will ever pass this check. Did you forget to set CA dir?\n");
		}

		if (stirshaken_vs_crl_dir.len > 0) {
			if (STIR_SHAKEN_STATUS_OK != stir_shaken_vs_load_crl_dir(ss, vs, stirshaken_vs_crl_dir.s)) {
				LM_ERR("Failed to init X509 cert store with CRL dir\n");
				stirshaken_print_error_details(ss);
				return -1;
			}
		} else {
			LM_WARN("Cert path check is turned on, but CRL dir is not set (it's not mandatory). Did you forget to set CRL dir?\n");
		}
	}

	return 0;
}

static int mod_init(void)
{
	stir_shaken_context_t	ss = { 0 };

	LM_INFO("Initialising STIR-Shaken\n");

	if (STIR_SHAKEN_STATUS_OK != stir_shaken_init(&ss, STIR_SHAKEN_LOGLEVEL_NOTHING)) {
		LM_ERR("Cannot init libstirshaken\n");
		stirshaken_print_error_details(&ss);
		return -1;
	}

	if (0 == start_as(&ss)) {
		if (stirshaken_as_default_key.len > 0) {
			LM_INFO("Authentication Service ready (with default key %s)\n", stirshaken_as_default_key.s);
		} else {
			LM_INFO("Authentication Service ready (without default key)\n");
		}
	} else {
		LM_WARN("Cannot start Authentication Service (%s)\n", stirshaken_as_default_key.len > 0 ? "with default key" : "without default key");
	}

	if (stirshaken_as_default_key.len == 0) {
		LM_WARN("Authentication Service using default key will not be available, because 'as_default_key' is not set (only authentication through w_stirshaken_add_identity_with_key is possible using a specific key)\n");
	}

	if (0 == start_vs(&ss)) {
		LM_INFO("Verification Service ready (%s)\n", stirshaken_vs_verify_x509_cert_path ? "with X509 cert path check" : "without X509 cert path check");
	} else {
		LM_WARN("Cannot start Verification Service (%s)\n", stirshaken_vs_verify_x509_cert_path ? "with X509 cert path check" : "without X509 cert path check");
		stir_shaken_vs_destroy(&vs);
	}

	if ((vs && !stirshaken_vs_verify_x509_cert_path) || !vs) {
		LM_WARN("A complete Shaken check with X509 certificate path verification will not be available (stirshaken_check_identity). "
				"Only checks of PASSporT decoding will be performed via stirshaken_check_identity_with_key() "
				"and stirshaken_check_identity_with_cert(). If you want complete check with downloaded certificate and/or X509 cert path verification "
				"then please set @vs_verify_x509_cert_path param to 1 and configure @vs_ca_dir (and optionally @vs_crl_dir)\n");
	}

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	LM_INFO("mod stirshaken child init\n");
	return 0;
}

static void mod_destroy(void)
{
	LM_INFO("mod stirshaken destroy\n");
	stir_shaken_as_destroy(&as);
	stir_shaken_vs_destroy(&vs);
	stir_shaken_deinit();
	return;
}

static int stirshaken_handle_cache(stir_shaken_context_t *ss, stir_shaken_passport_t *passport, stir_shaken_cert_t *cert)
{
	if (!passport || !cert)
		return -1;

	LM_DBG("Handling certificate cache...\n");

	if (!stirshaken_vs_cache_dir.len) {
		LM_ERR("Cache dir not set\n");
		return -1;
	}

	if (!ss->cert_fetched_from_cache) {

		// save certificate to cache with url as a key 
		char cert_full_path[STIR_SHAKEN_BUFLEN] = { 0 };
		const char *x5u = stir_shaken_passport_get_header(ss, passport, "x5u");

		if (stir_shaken_zstr(x5u)) {

			// This should never happen as stir_shaken_sih_verify returns error in such case

			LM_ERR("PASSporT has no x5u\n");
			return -1;
		}

		if (-1 == get_cert_name_hashed(x5u, cert_full_path, STIR_SHAKEN_BUFLEN)) {
			LM_ERR("Cannot get cert name hashed\n");
			return -1;
		}

		LM_DBG("Checking for presence of expired version of freshly downloaded certificate %s in cache (looking for name: %s)\n", x5u, cert_full_path);

		if (STIR_SHAKEN_STATUS_OK == stir_shaken_file_exists(cert_full_path)) {

			LM_DBG("Expired version of certificate %s found in cache (with name: %s). Removing it...\n", x5u, cert_full_path);

			if (STIR_SHAKEN_STATUS_OK != stir_shaken_file_remove(cert_full_path)) {
				LM_ERR("Couldn't remove certificate %s from cache\n", cert_full_path);
				return -1;
			}
		}

#ifdef STIR_SHAKEN_CAN_RW_X509_FULLCHAIN
		LM_DBG("Saving fresh certificate+chain %s to cache as %s\n", x5u, cert_full_path);
		if (STIR_SHAKEN_STATUS_OK != stir_shaken_x509_to_disk_fullchain(ss, cert->x, cert->xchain, cert_full_path)) {
#else
		LM_DBG("Saving fresh certificate %s to cache as %s\n", x5u, cert_full_path);
		if (STIR_SHAKEN_STATUS_OK != stir_shaken_x509_to_disk(ss, cert->x, cert_full_path)) {
#endif
			LM_ERR("Failed to cache certificate %s to disk", x5u);
		}

	} else {
		LM_DBG("Certificate was fetched from cache, so skipping saving it\n");
	}

	return 0;
}

#define STIRSHAKEN_HDR_IDENTITY "Identity"
#define STIRSHAKEN_HDR_IDENTITY_LEN (sizeof(STIRSHAKEN_HDR_IDENTITY) - 1)

static int ki_stirshaken_check_identity(sip_msg_t *msg)
{
	str ibody = STR_NULL;
	hdr_field_t *hf = NULL;

	stir_shaken_context_t ss = { 0 };
	stir_shaken_passport_t *passport_out = NULL;
	stir_shaken_cert_t *cert_out = NULL;

	for (hf = msg->headers; hf; hf = hf->next) {
		if (hf->name.len == STIRSHAKEN_HDR_IDENTITY_LEN
				&& strncasecmp(hf->name.s, STIRSHAKEN_HDR_IDENTITY,
					STIRSHAKEN_HDR_IDENTITY_LEN) == 0)
			break;
	}

	if (hf == NULL) {
		LM_DBG("no identity header\n");
		goto fail;
	}

	ibody = hf->body;

	if (STIR_SHAKEN_STATUS_OK != stir_shaken_vs_sih_verify(&ss, vs, ibody.s, &cert_out, &passport_out)) {
		LM_ERR("SIP Identity Header did not pass verification: %s", stir_shaken_get_error(&ss, NULL));

		stirshaken_print_error_details(&ss);
		goto fail;
	}

	if (stirshaken_vs_cache_certificates) {
		stirshaken_handle_cache(&ss, passport_out, cert_out);
	}

	// Check that PASSporT applies to the current moment in time
	if (STIR_SHAKEN_STATUS_OK != stir_shaken_passport_validate_iat_against_freshness(&ss, passport_out, stirshaken_vs_identity_expire_s)) {

		stir_shaken_error_t error_code = 0;
		stir_shaken_get_error(&ss, &error_code);

		if (error_code == STIR_SHAKEN_ERROR_PASSPORT_INVALID_IAT_VALUE_FUTURE) {
			LM_ERR("PASSporT not valid yet\n");
		} else if (error_code == STIR_SHAKEN_ERROR_PASSPORT_INVALID_IAT_VALUE_EXPIRED) {
			LM_ERR("PASSporT expired\n");
		} else if (error_code == STIR_SHAKEN_ERROR_PASSPORT_INVALID_IAT) {
			LM_ERR("PASSporT is missing @iat grant\n");
		}

		LM_ERR("PASSporT doesn't apply to the current moment in time\n");
		goto fail;
	}

	if (stirshaken_vs_verify_x509_cert_path) {

		LM_DBG("Running X509 certificate path verification\n");

		if (!vs) {
			LM_ERR("Verification Service not started\n");
			goto fail;
		}

		if (STIR_SHAKEN_STATUS_OK != stir_shaken_verify_cert_path(&ss, cert_out, vs->store)) {
			LM_ERR("Cert did not pass X509 path validation\n");
			stirshaken_print_error_details(&ss);
			goto fail;
		}
	}

	LM_DBG("identity check: ok (%s)\n", ss.x509_cert_path_checked ? "with X509 cert path check" : "without X509 cert path check");
	stir_shaken_passport_destroy(&passport_out);
	stir_shaken_cert_destroy(&cert_out);
	return 1;

fail:
	stir_shaken_passport_destroy(&passport_out);
	stir_shaken_cert_destroy(&cert_out);
	LM_ERR("identity check: fail\n");
	return -1;
}

/**
 * Verify SIP Identity Header (involves call from libstirshaken to cache_callback,
 * wich will supply requested certificate from cache [if configured to do so]
 * or will let libstirshaken to perform HTTP(s) GET request to download certificate).
 * Verify a call with STIR-Shaken.
 *
 * This method checks if SIP Identity Header covers semantically valid PASSporT.
 * This method consults cache_callback in an attempt to obtain certificate that is referenced in PASSporT's x5u header.
 * This method checks if PASSporT verifies successfully with a public key retrieved from obtained certificate.
 * Optionally (if Verification Service is configured to do so with stirshaken_vs_verify_x509_cert_path param set to 1)
 * this method checks if certificate is trusted, by execution of X509 certificate path check.
 * 
 * Optionally:
 * 		- retrieve PASSporT from SIP Identity Header
 * 		- retrieve certificate referenced in PASSporT's x5u header
 * 		- cache certificate
 *
 * Kamailio config usage example:
 *
 *		stirshaken_check_identity();
 */
static int w_stirshaken_check_identity(sip_msg_t *msg, char *str1, char *str2)
{
	LM_INFO("identity check\n");

	if (!vs) {
		LM_ERR("Cannot perform identity check involving certificate downloading, caching, or X509 cert pach checking, "
				"because Verification Service is not running. Please turn on and configure Verification Service, "
				"otherwise only stirshaken_check_identity_with_cert() and stirshaken_check_identity_with_key() "
				"methods will be available. Check log file for module's initialisation errors\n");
		return -1;
	}

	return ki_stirshaken_check_identity(msg);
}

static int ki_stirshaken_check_identity_with_cert(sip_msg_t *msg, str *cert_path)
{
	str ibody = STR_NULL;
	hdr_field_t *hf = NULL;

	stir_shaken_context_t ss = { 0 };
	stir_shaken_passport_t *passport_out = NULL;
	stir_shaken_cert_t cert = { 0 };

	for (hf = msg->headers; hf; hf = hf->next) {
		if (hf->name.len == STIRSHAKEN_HDR_IDENTITY_LEN
				&& strncasecmp(hf->name.s, STIRSHAKEN_HDR_IDENTITY,
					STIRSHAKEN_HDR_IDENTITY_LEN) == 0)
			break;
	}

	if (hf == NULL) {
		LM_DBG("no identity header\n");
		goto fail;
	}

	ibody = hf->body;

	if (!(cert.x = stir_shaken_load_x509_from_file(&ss, cert_path->s))) {
		LM_DBG("Cannot load X509 from file\n");
		stirshaken_print_error_details(&ss);
		goto fail;
	}

	if (STIR_SHAKEN_STATUS_OK != stir_shaken_sih_verify_with_cert(&ss, ibody.s, &cert, &passport_out)) {
		LM_ERR("SIP Identity Header did not pass verification against certificate\n");
		stirshaken_print_error_details(&ss);
		goto fail;
	}

	// Check that PASSporT applies to the current moment in time
	if (STIR_SHAKEN_STATUS_OK != stir_shaken_passport_validate_iat_against_freshness(&ss, passport_out, stirshaken_vs_identity_expire_s)) {

		stir_shaken_error_t error_code = 0;
		stir_shaken_get_error(&ss, &error_code);

		if (error_code == STIR_SHAKEN_ERROR_PASSPORT_INVALID_IAT_VALUE_FUTURE) {
			LM_ERR("PASSporT not valid yet\n");
		} else if (error_code == STIR_SHAKEN_ERROR_PASSPORT_INVALID_IAT_VALUE_EXPIRED) {
			LM_ERR("PASSporT expired\n");
		} else if (error_code == STIR_SHAKEN_ERROR_PASSPORT_INVALID_IAT) {
			LM_ERR("PASSporT is missing @iat grant\n");
		}

		LM_ERR("PASSporT doesn't apply to the current moment in time\n");
		goto fail;
	}

	LM_DBG("identity check: ok (%s)\n", ss.x509_cert_path_checked ? "with X509 cert path check" : "without X509 cert path check");

	stir_shaken_passport_destroy(&passport_out);
	stir_shaken_cert_deinit(&cert);
	return 1;

fail:
	stir_shaken_passport_destroy(&passport_out);
	stir_shaken_cert_deinit(&cert);
	LM_ERR("identity check: fail\n");
	return -1;
}

/**
 * Verify SIP Identity Header against specified certificate (does not involve HTTP(s) GET request).
 * WARNING:
 * This method only checks if SIP Identity Header was signed by a key from certificate given as argument.
 * This method doesn't attempt to obtain certificate referenced in PASSporT (but PASSporT should be checked with key corresponding to that certificate).
 * Therefore it is possible that this check will be successful, while PASSporT is not valid (could be signed with key that doesn't match certificate referenced in x5u header).
 * If you want a complete Shaken check or if you are not sure what you're doing, then you should execute w_stirshaken_check_identity() instead
 * (and configure Verification Service to perform X509 certificate path verification with stirshaken_vs_verify_x509_cert_path param set to 1).
 *
 * Kamailio config usage example:
 *
 *		stirshaken_check_identity_with_cert("/path/to/cert");
 */
static int w_stirshaken_check_identity_with_cert(sip_msg_t *msg, char *cert_path, char *str2)
{
	str keyval = STR_NULL;

	LM_INFO("identity check using certificate\n");

	if(fixup_get_svalue(msg, (gparam_t*)cert_path, &keyval)<0) {
		LM_ERR("failed to get certificate path parameter\n");
		return -1;
	}

	return ki_stirshaken_check_identity_with_cert(msg, &keyval);
}

static int ki_stirshaken_check_identity_with_key(sip_msg_t *msg, str *keypath)
{
	str ibody = STR_NULL;
	hdr_field_t *hf = NULL;

	stir_shaken_context_t ss = { 0 };
	stir_shaken_passport_t *passport_out = NULL;
	unsigned char key[STIR_SHAKEN_PUB_KEY_RAW_BUF_LEN] = { 0 };
	uint32_t key_len = STIR_SHAKEN_PUB_KEY_RAW_BUF_LEN;

	for (hf = msg->headers; hf; hf = hf->next) {
		if (hf->name.len == STIRSHAKEN_HDR_IDENTITY_LEN
				&& strncasecmp(hf->name.s, STIRSHAKEN_HDR_IDENTITY,
					STIRSHAKEN_HDR_IDENTITY_LEN) == 0)
			break;
	}

	if (hf == NULL) {
		LM_DBG("no identity header\n");
		goto fail;
	}

	ibody = hf->body;

	if (keypath && keypath->s) {

		if (STIR_SHAKEN_STATUS_OK != stir_shaken_load_key_raw(&ss, keypath->s, key, &key_len)) {
			LM_ERR("Failed to load private key\n");
			stirshaken_print_error_details(&ss);
			goto fail;
		}
	}

	if (STIR_SHAKEN_STATUS_OK != stir_shaken_sih_verify_with_key(&ss, ibody.s, key, key_len, &passport_out)) {
		LM_ERR("SIP Identity Header did not pass verification against key\n");
		stirshaken_print_error_details(&ss);
		goto fail;
	}

	// We can do something with PASSporT here or just realease it
	stir_shaken_passport_destroy(&passport_out);

	LM_DBG("identity check: ok (%s)\n", ss.x509_cert_path_checked ? "with X509 cert path check" : "without X509 cert path check");
	return 1;

fail:
	stir_shaken_passport_destroy(&passport_out);
	LM_ERR("identity check: fail\n");
	return -1;
}

/**
 * Verify SIP Identity Header against specified public key (does not involve HTTP(s) GET request).
 * WARNING:
 * This method only checks if SIP Identity Header was signed by a key corresponding to specified public key.
 * This method doesn't attempt to obtain certificate referenced in PASSporT (but PASSporT should be checked with key corresponding to that certificate).
 * Therefore it is possible that this check will be successful, while PASSporT is not valid (could be signed with key that doesn't match certificate referenced in x5u header).
 * If you want a complete Shaken check or if you are not sure what you're doing, then you should execute w_stirshaken_check_identity() instead
 * (and configure Verification Service to perform X509 certificate path verification with stirshaken_vs_verify_x509_cert_path param set to 1).
 *
 * Kamailio config usage example:
 *
 *		stirshaken_check_identity_with_key("/path/to/key");
 */
static int w_stirshaken_check_identity_with_key(sip_msg_t *msg, char *pkey_path, char *str2)
{
	str keypath = STR_NULL;

	LM_INFO("identity check using public key\n");

	if(fixup_get_svalue(msg, (gparam_t*)pkey_path, &keypath)<0) {
		LM_ERR("failed to get key path parameter\n");
		return -1;
	}

	return ki_stirshaken_check_identity_with_key(msg, &keypath);
}

static int ki_stirshaken_add_identity_with_key(sip_msg_t *msg, str *x5u, str *attest,
		str *origtn_val, str *desttn_val, str *origid, str *keypath)
{
	stir_shaken_context_t ss = { 0 };
	char *sih = NULL;
	stir_shaken_passport_t *passport = NULL;
	str ibody = STR_NULL;
	str hdr = STR_NULL;
	sr_lump_t *anchor = NULL;
	stir_shaken_passport_params_t params = {
		.x5u = x5u ? x5u->s : NULL,
		.attest = attest ? attest->s : NULL,
		.desttn_key = "tn",
		.desttn_val = desttn_val ? desttn_val->s : NULL,
		.iat = time(NULL),
		.origtn_key = "tn",
		.origtn_val = origtn_val ? origtn_val->s : NULL,
		.origid = origid ? origid->s : NULL
	};
	char uuid_str[37] = { 0 };

	if (!params.origid || !strlen(params.origid)) {

		uuid_t uuid;

		uuid_generate(uuid);
		uuid_unparse_lower(uuid, uuid_str);
		params.origid = uuid_str;
	}

	if (keypath && keypath->s) {

		unsigned char key[STIR_SHAKEN_PRIV_KEY_RAW_BUF_LEN];
		uint32_t key_len = STIR_SHAKEN_PRIV_KEY_RAW_BUF_LEN;

		if (STIR_SHAKEN_STATUS_OK != stir_shaken_load_key_raw(&ss, keypath->s, key, &key_len)) {
			LM_ERR("Failed to load private key\n");
			stirshaken_print_error_details(&ss);
			goto error;
		}

		sih = stir_shaken_authenticate_to_sih_with_key(&ss, &params, &passport, key, key_len);
		if (!sih) {
			LM_ERR("Failed to create SIP Identity Header with key %s\n", keypath->s);
			stirshaken_print_error_details(&ss);
			goto error;
		}

	} else {

		if (!as) {
			LM_ERR("Authentication Service not started. Is default key set for Authentication Service? Please check @as_default_key param\n");
			goto error;
		}

		sih = stir_shaken_as_authenticate_to_sih(&ss, as, &params, &passport);
		if (!sih) {
			LM_ERR("Failed to create SIP Identity Header with default key. Is a default key set for Authentication Service? Please check @as_default_key param\n");
			stirshaken_print_error_details(&ss);
			goto error;
		}
	}

	// We can do something with PASSporT here or just realease it
	stir_shaken_passport_destroy(&passport);

	ibody.s = sih;
	ibody.len = strlen(sih);

	if (ibody.len <= 0) {
		LM_DBG("Got SIP Identity Header with 0 length\n");
		goto error;
	}

	LM_DBG("appending identity: %.*s\n", ibody.len, ibody.s);
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		goto error;
	}

	hdr.len = STIRSHAKEN_HDR_IDENTITY_LEN + 1 + 1 + ibody.len + 2;
	hdr.s = (char*)pkg_malloc(hdr.len + 1);
	if (hdr.s == NULL) {
		PKG_MEM_ERROR;
		goto error;
	}
	memcpy(hdr.s, STIRSHAKEN_HDR_IDENTITY, STIRSHAKEN_HDR_IDENTITY_LEN);
	*(hdr.s + STIRSHAKEN_HDR_IDENTITY_LEN) = ':';
	*(hdr.s + STIRSHAKEN_HDR_IDENTITY_LEN + 1) = ' ';

	memcpy(hdr.s + STIRSHAKEN_HDR_IDENTITY_LEN + 2, ibody.s, ibody.len);
	*(hdr.s + STIRSHAKEN_HDR_IDENTITY_LEN + ibody.len + 2) = '\r';
	*(hdr.s + STIRSHAKEN_HDR_IDENTITY_LEN + ibody.len + 3) = '\n';

	/* anchor after last header */
	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if((anchor == NULL)
			|| (insert_new_lump_before(anchor, hdr.s, hdr.len, 0) == 0)) {
		LM_ERR("cannot insert identity header\n");
		pkg_free(hdr.s);
		goto error;
	}

	if(sih) {
		free(sih);
	}
	return 1;

error:
	stir_shaken_passport_destroy(&passport);
	if(sih) {
		free(sih);
	}
	return -1;
}

/**
 * Add SIP Identity Header to the call using specific key. Authenticate call with STIR-Shaken.
 *
 * Kamailio config usage example:
 *
 * 		stirshaken_add_identity_with_key("https://sp.com/sp.pem", "B", "+44100", "+44200", "ref", "/path/to/key");
 */
static int w_stirshaken_add_identity_with_key(sip_msg_t *msg, str *px5u, str *pattest,
		str *porigtn_val, str *pdesttn_val, str *porigid, str *pkeypath)
{
	str x5u = STR_NULL;
	str attest = STR_NULL;
	str origtn_val = STR_NULL;
	str desttn_val = STR_NULL;
	str origid = STR_NULL;
	str keypath = STR_NULL;

	if (fixup_get_svalue(msg, (gparam_t*)px5u, &x5u)<0) {
		LM_ERR("failed to get x5u parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)pattest, &attest)<0) {
		LM_ERR("failed to get attest parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)porigtn_val, &origtn_val)<0) {
		LM_ERR("failed to get origtn_val parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)pdesttn_val, &desttn_val)<0) {
		LM_ERR("failed to get desttn_val parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)porigid, &origid)<0) {
		LM_ERR("failed to get origid parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)pkeypath, &keypath)<0) {
		LM_ERR("failed to get keypath parameter\n");
		return -1;
	}

	return ki_stirshaken_add_identity_with_key(msg, &x5u, &attest, &origtn_val, &desttn_val, &origid, &keypath);
}

static int ki_stirshaken_add_identity(sip_msg_t *msg, str *x5u, str *attest, str *origtn_val, str *desttn_val, str *origid)
{
	return ki_stirshaken_add_identity_with_key(msg, x5u, attest, origtn_val, desttn_val, origid, NULL);
}

/**
 * Add SIP Identity Header to the call using default private key. Authenticate call with STIR-Shaken.
 *
 * Kamailio config usage example:
 *
 * 		stirshaken_add_identity("https://sp.com/sp.pem", "B", "+44100", "+44200", "ref");
 */
static int w_stirshaken_add_identity(sip_msg_t *msg, str *px5u, str *pattest, str *porigtn_val, str *pdesttn_val, str *porigid)
{
	str x5u = STR_NULL;
	str attest = STR_NULL;
	str origtn_val = STR_NULL;
	str desttn_val = STR_NULL;
	str origid = STR_NULL;

	if (fixup_get_svalue(msg, (gparam_t*)px5u, &x5u)<0) {
		LM_ERR("failed to get x5u parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)pattest, &attest)<0) {
		LM_ERR("failed to get attest parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)porigtn_val, &origtn_val)<0) {
		LM_ERR("failed to get origtn_val parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)pdesttn_val, &desttn_val)<0) {
		LM_ERR("failed to get desttn_val parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)porigid, &origid)<0) {
		LM_ERR("failed to get origid parameter\n");
		return -1;
	}

	return ki_stirshaken_add_identity(msg, &x5u, &attest, &origtn_val, &desttn_val, &origid);
}


/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_stirshaken_exports[] = {
	{ str_init("stirshaken"), str_init("stirshaken_check_identity"),
		SR_KEMIP_INT, ki_stirshaken_check_identity,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("stirshaken"), str_init("stirshaken_check_identity_with_cert"),
		SR_KEMIP_INT, ki_stirshaken_check_identity_with_cert,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("stirshaken"), str_init("stirshaken_check_identity_with_key"),
		SR_KEMIP_INT, ki_stirshaken_check_identity_with_key,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("stirshaken"), str_init("stirshaken_add_identity"),
		SR_KEMIP_INT, ki_stirshaken_add_identity,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE }
	},
	{ str_init("stirshaken"), str_init("stirshaken_add_identity_with_key"),
		SR_KEMIP_INT, ki_stirshaken_add_identity_with_key,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR }
	},
	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_stirshaken_exports);
	return 0;
}
