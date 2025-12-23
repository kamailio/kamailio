/*
 * Arnacon Authentication Module for Kamailio
 *
 * Copyright (C) 2025 Jonathan Kandel
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/mod_fix.h"
#include "../../core/pvar.h"
#include "../../core/sr_module.h"
#include "../../core/ut.h"
#include "../../modules/auth/api.h"

#include "arnacon_core.h"
#include "auth_arnacon.h"

MODULE_VERSION

/* Module parameters */
static char *arnacon_ens_registry_address =
		"0x16742E546bF92118F7dfdbEF5170E44C47ae254b";
static char *arnacon_rpc_url = "https://polygon-rpc.com";
static int arnacon_signature_timeout = 30;
static int arnacon_debug_mode = 0;

/* Forward declarations */
static int mod_init(void);
static int child_init(int rank);
static void destroy(void);
static int arnacon_authenticate_user(struct sip_msg *msg, char *ens_name_param,
		char *x_data_param, char *x_sign_param);
static int arnacon_user_exists(
		struct sip_msg *msg, char *ens_name_param, char *str2);

/* Fixup function declarations */
static int fixup_arnacon_auth(void **param, int param_no);
static int fixup_arnacon_auth_free(void **param, int param_no);
static int fixup_arnacon_exists(void **param, int param_no);
static int fixup_arnacon_exists_free(void **param, int param_no);

/* Module interface */
static cmd_export_t cmds[] = {
		{"arnacon_authenticate", (cmd_function)arnacon_authenticate_user, 3,
				fixup_arnacon_auth, fixup_arnacon_auth_free, REQUEST_ROUTE},
		{"arnacon_user_exists", (cmd_function)arnacon_user_exists, 1,
				fixup_arnacon_exists, fixup_arnacon_exists_free, REQUEST_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {
		{"ens_registry_address", PARAM_STRING, &arnacon_ens_registry_address},
		{"rpc_url", PARAM_STRING, &arnacon_rpc_url},
		{"signature_timeout", PARAM_INT, &arnacon_signature_timeout},
		{"debug_mode", PARAM_INT, &arnacon_debug_mode}, {0, 0, 0}};

struct module_exports exports = {
		"auth_arnacon",	 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* exported RPC functions */
		0,				 /* exported pseudo-variables */
		0,				 /* response function */
		mod_init,		 /* module initialization function */
		child_init,		 /* per child init function */
		destroy			 /* destroy function */
};

/**
 * Module initialization function
 */
static int mod_init(void)
{
	LM_INFO("auth_arnacon: Initializing...\n");

	if(arnacon_debug_mode) {
		LM_INFO("auth_arnacon: Debug mode enabled\n");
		LM_INFO("auth_arnacon: ENS Registry: %s\n",
				arnacon_ens_registry_address);
		LM_INFO("auth_arnacon: RPC URL: %s\n", arnacon_rpc_url);
		LM_INFO("auth_arnacon: Signature timeout: %d seconds\n",
				arnacon_signature_timeout);
		LM_INFO("auth_arnacon: Name Wrapper detection: Dynamic\n");
	}

	/* Initialize curl globally */
	if(arnacon_core_init() != 0) {
		LM_ERR("auth_arnacon: Failed to initialize core\n");
		return -1;
	}

	LM_INFO("auth_arnacon: Initialized successfully\n");
	return 0;
}

/**
 * Child initialization function
 */
static int child_init(int rank)
{
	LM_DBG("auth_arnacon: Child process %d initialized\n", rank);
	return 0;
}

/**
 * Module destroy function
 */
static void destroy(void)
{
	LM_INFO("auth_arnacon: Shutting down\n");
	arnacon_core_cleanup();
}

/**
 * Fixup function for arnacon_authenticate parameters
 * Converts string parameters to fparam_t that can handle both
 * literal strings and pseudo-variables
 */
static int fixup_arnacon_auth(void **param, int param_no)
{
	if(param_no >= 1 && param_no <= 3) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}

/**
 * Free fixup function for arnacon_authenticate
 */
static int fixup_arnacon_auth_free(void **param, int param_no)
{
	if(param_no >= 1 && param_no <= 3) {
		return fixup_free_spve_null(param, 1);
	}
	return 0;
}

/**
 * Fixup function for arnacon_user_exists parameter
 */
static int fixup_arnacon_exists(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}

/**
 * Free fixup function for arnacon_user_exists
 */
static int fixup_arnacon_exists_free(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_free_spve_null(param, 1);
	}
	return 0;
}

/**
 * Helper function to get string value from fixup parameter
 * Uses Kamailio's core fixup_get_svalue for proper PV handling
 */
static int arnacon_get_str_param(struct sip_msg *msg, gparam_t *gp, str *result)
{
	if(!gp || !result) {
		LM_ERR("Invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, gp, result) < 0) {
		LM_ERR("Failed to get string value from parameter\n");
		return -1;
	}

	if(result->s == NULL || result->len == 0) {
		LM_ERR("Parameter resolved to empty string\n");
		return -1;
	}

	if(arnacon_debug_mode) {
		LM_DBG("Resolved parameter to: %.*s\n", result->len, result->s);
	}

	return 0;
}

/**
 * Extract X-Data and X-Sign headers from SIP message
 */
static int extract_auth_headers(struct sip_msg *msg, str *x_data, str *x_sign)
{
	struct hdr_field *hdr;

	/* Initialize output parameters */
	x_data->s = NULL;
	x_data->len = 0;
	x_sign->s = NULL;
	x_sign->len = 0;

	/* Look for X-Data and X-Sign headers */
	for(hdr = msg->headers; hdr; hdr = hdr->next) {
		if(hdr->type == HDR_OTHER_T) {
			if(hdr->name.len == 6
					&& strncasecmp(hdr->name.s, "X-Data", 6) == 0) {
				/* Use trim_len from core/ut.h to trim whitespace */
				trim_len(x_data->len, x_data->s, hdr->body);
			} else if(hdr->name.len == 6
					  && strncasecmp(hdr->name.s, "X-Sign", 6) == 0) {
				/* Use trim_len from core/ut.h to trim whitespace */
				trim_len(x_sign->len, x_sign->s, hdr->body);
			}
		}
	}

	if(x_data->len == 0 || x_sign->len == 0) {
		LM_ERR("Missing required headers: X-Data=%d, X-Sign=%d\n",
				x_data->len > 0, x_sign->len > 0);
		return -1;
	}

	return 0;
}

/**
 * Main authentication function
 * Usage: arnacon_authenticate("ens_name", "x_data", "x_sign")
 * Or: arnacon_authenticate("$fU", "", "") - to extract from headers
 */
static int arnacon_authenticate_user(struct sip_msg *msg, char *ens_name_param,
		char *x_data_param, char *x_sign_param)
{
	str ens_name, x_data, x_sign;
	char ens_name_buf[256], x_data_buf[256], x_sign_buf[256];
	int result;
	int use_headers;

	if(arnacon_debug_mode) {
		LM_DBG("Authenticate function called\n");
	}

	/* Extract ENS name parameter using Kamailio's fixup_get_svalue */
	if(arnacon_get_str_param(msg, (gparam_t *)ens_name_param, &ens_name) != 0) {
		LM_ERR("Failed to get ENS name parameter\n");
		return -1;
	}

	/* Check if we should extract headers automatically
   * Try to get x_data and x_sign parameters; if empty, use headers */
	use_headers = 0;
	if(arnacon_get_str_param(msg, (gparam_t *)x_data_param, &x_data) != 0
			|| x_data.len == 0) {
		use_headers = 1;
	}
	if(!use_headers
			&& (arnacon_get_str_param(msg, (gparam_t *)x_sign_param, &x_sign)
							!= 0
					|| x_sign.len == 0)) {
		use_headers = 1;
	}

	if(use_headers) {
		/* Extract from SIP headers */
		if(extract_auth_headers(msg, &x_data, &x_sign) != 0) {
			LM_ERR("Failed to extract X-Data and X-Sign headers\n");
			return -1;
		}
	}

	/* Copy to null-terminated buffers */
	if(ens_name.len >= sizeof(ens_name_buf) || x_data.len >= sizeof(x_data_buf)
			|| x_sign.len >= sizeof(x_sign_buf)) {
		LM_ERR("Parameter too long: ens_name=%d, x_data=%d, x_sign=%d\n",
				ens_name.len, x_data.len, x_sign.len);
		return -1;
	}

	memcpy(ens_name_buf, ens_name.s, ens_name.len);
	ens_name_buf[ens_name.len] = '\0';

	memcpy(x_data_buf, x_data.s, x_data.len);
	x_data_buf[x_data.len] = '\0';

	memcpy(x_sign_buf, x_sign.s, x_sign.len);
	x_sign_buf[x_sign.len] = '\0';

	if(arnacon_debug_mode) {
		LM_DBG("Authentication request: ENS=%s\n", ens_name_buf);
		LM_DBG("X-Data: %s\n", x_data_buf);
		LM_DBG("X-Sign: %s\n", x_sign_buf);
	}

	/* Call core authentication function */
	result = arnacon_core_authenticate(ens_name_buf, x_data_buf, x_sign_buf,
			arnacon_ens_registry_address, arnacon_rpc_url,
			arnacon_signature_timeout, arnacon_debug_mode);

	if(arnacon_debug_mode) {
		LM_DBG("Authentication result: %d\n", result);
	}

	/* Return 1 for success, -1 for failure (Kamailio convention) */
	return (result == ARNACON_SUCCESS) ? 1 : -1;
}

/**
 * Check if user exists (ENS has an owner)
 * Usage: arnacon_user_exists("ens_name")
 */
static int arnacon_user_exists(
		struct sip_msg *msg, char *ens_name_param, char *str2)
{
	str ens_name;
	char ens_name_buf[256];
	int result;

	(void)str2; /* Unused parameter */

	if(arnacon_debug_mode) {
		LM_DBG("User exists function called\n");
	}

	/* Extract ENS name parameter using Kamailio's fixup_get_svalue */
	if(arnacon_get_str_param(msg, (gparam_t *)ens_name_param, &ens_name) != 0) {
		LM_ERR("Failed to get ENS name parameter\n");
		return -1;
	}

	/* Copy to null-terminated buffer */
	if(ens_name.len >= sizeof(ens_name_buf)) {
		LM_ERR("ENS name too long: %d (max: %zu)\n", ens_name.len,
				sizeof(ens_name_buf) - 1);
		return -1;
	}

	memcpy(ens_name_buf, ens_name.s, ens_name.len);
	ens_name_buf[ens_name.len] = '\0';

	if(arnacon_debug_mode) {
		LM_DBG("Checking if user exists: ENS=%s\n", ens_name_buf);
	}

	/* Call core user exists function */
	result = arnacon_core_user_exists(ens_name_buf,
			arnacon_ens_registry_address, arnacon_rpc_url, arnacon_debug_mode);

	if(arnacon_debug_mode) {
		LM_DBG("User exists result: %d\n", result);
	}

	/* Return 1 if user exists, -1 if not */
	return (result == 1) ? 1 : -1;
}
