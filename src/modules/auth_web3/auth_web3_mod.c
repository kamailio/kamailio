/*
 * Web3 Digest Authentication Module - Complete Drop-in Replacement
 * Based on original Kamailio auth module with blockchain-powered authentication
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2025 Jonathan Kandel
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <curl/curl.h>
#include <stdint.h>
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/error.h"
#include "../../core/ut.h"
#include "../../core/pvapi.h"
#include "../../core/lvalue.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../../core/rand/kam_rand.h"
#include "../../modules/sl/sl.h"
#include "auth_web3_mod.h"
#include "challenge.h"
#include "api.h"
#include "nid.h"
#include "nc.h"
#include "ot_nonce.h"
#include "rfc2617.h"
#include "rfc2617_sha256.h"

MODULE_VERSION

#define RAND_SECRET_LEN 32

/* ===== WEB3 ADDITIONS START ===== */
#define DEFAULT_WEB3_RPC_URL "https://testnet.sapphire.oasis.dev"
#define DEFAULT_WEB3_CONTRACT_ADDRESS \
	"0xE773BB79689379d32Ad1Db839868b6756B493aea"

/* Web3-specific parameters */
static char *web3_rpc_url = DEFAULT_WEB3_RPC_URL;
static char *web3_contract_address = DEFAULT_WEB3_CONTRACT_ADDRESS;
static int web3_debug_mode = 1;
static int web3_timeout = 10;

/* Web3 response structure */
struct Web3ResponseData
{
	char *memory;
	size_t size;
};

/* Web3 function prototypes */
static int auth_web3_check_response(dig_cred_t *cred, str *method, char *ha1);
static size_t web3_curl_callback(void *contents, size_t size, size_t nmemb,
		struct Web3ResponseData *data);

/* Function to convert hex string to bytes */
static int hex_to_bytes(
		const char *hex_str, unsigned char *bytes, int max_bytes)
{
	int len = strlen(hex_str);
	if(len % 2 != 0)
		return -1; // Invalid hex string

	int byte_len = len / 2;
	if(byte_len > max_bytes)
		return -1; // Too many bytes

	for(int i = 0; i < byte_len; i++) {
		char hex_byte[3] = {hex_str[i * 2], hex_str[i * 2 + 1], '\0'};
		bytes[i] = (unsigned char)strtol(hex_byte, NULL, 16);
	}

	return byte_len;
}

/* Simplified Web3 implementation */
static size_t web3_curl_callback(void *contents, size_t size, size_t nmemb,
		struct Web3ResponseData *data)
{
	size_t realsize = size * nmemb;
	char *ptr = realloc(data->memory, data->size + realsize + 1);
	if(!ptr)
		return 0;

	data->memory = ptr;
	memcpy(&(data->memory[data->size]), contents, realsize);
	data->size += realsize;
	data->memory[data->size] = 0;
	return realsize;
}

/* 
 * WEB3 REPLACEMENT: This replaces the original auth_check_response() 
 * This is the ONLY function that changes - everything else stays the same!
 */
static int auth_web3_check_response(dig_cred_t *cred, str *method, char *ha1)
{
	CURL *curl;
	CURLcode res;
	struct Web3ResponseData web3_response = {0};
	struct curl_slist *headers = NULL;
	int result = NOT_AUTHENTICATED;
	char username_str[256];
	char *call_data = NULL;

	// Extract username from credentials
	if(cred->username.user.len >= sizeof(username_str)) {
		LM_ERR("Web3Auth: Username too long (%d chars)\n",
				cred->username.user.len);
		return NOT_AUTHENTICATED;
	}

	memcpy(username_str, cred->username.user.s, cred->username.user.len);
	username_str[cred->username.user.len] = '\0';

	if(web3_debug_mode) {
		LM_INFO("Web3Auth: Authenticating user=%s, realm=%.*s\n", username_str,
				cred->realm.len, cred->realm.s);
		LM_INFO("Web3Auth: User provided response=%.*s\n", cred->response.len,
				cred->response.s);
	}

	curl = curl_easy_init();
	if(!curl) {
		LM_ERR("Web3Auth: Failed to initialize curl\n");
		return NOT_AUTHENTICATED;
	}

	// Extract parameters from SIP message context
	char realm_str[256], method_str[16], uri_str[256], nonce_str[256],
			response_str[256];

	// Extract realm
	if(cred->realm.len >= sizeof(realm_str)) {
		LM_ERR("Web3Auth: Realm too long (%d chars)\n", cred->realm.len);
		goto cleanup;
	}
	memcpy(realm_str, cred->realm.s, cred->realm.len);
	realm_str[cred->realm.len] = '\0';

	// Extract method (from the method parameter)
	if(method && method->len < sizeof(method_str)) {
		memcpy(method_str, method->s, method->len);
		method_str[method->len] = '\0';
	} else {
		strcpy(method_str, "REGISTER"); // Default method
	}

	// Extract URI and nonce from digest credentials
	if(cred->uri.len >= sizeof(uri_str)) {
		LM_ERR("Web3Auth: URI too long (%d chars)\n", cred->uri.len);
		goto cleanup;
	}
	memcpy(uri_str, cred->uri.s, cred->uri.len);
	uri_str[cred->uri.len] = '\0';

	if(cred->nonce.len >= sizeof(nonce_str)) {
		LM_ERR("Web3Auth: Nonce too long (%d chars)\n", cred->nonce.len);
		goto cleanup;
	}
	memcpy(nonce_str, cred->nonce.s, cred->nonce.len);
	nonce_str[cred->nonce.len] = '\0';

	// Extract user's response
	if(cred->response.len >= sizeof(response_str)) {
		LM_ERR("Web3Auth: Response too long (%d chars)\n", cred->response.len);
		goto cleanup;
	}
	memcpy(response_str, cred->response.s, cred->response.len);
	response_str[cred->response.len] = '\0';

	// Determine algorithm: 0 for MD5, 1 for SHA256, 2 for SHA512
	uint8_t algo = 0; // Default to MD5
	if(auth_algorithm.len > 0) {
		if(auth_algorithm.len == 7
				&& strncmp(auth_algorithm.s, "SHA-256", 7) == 0) {
			algo = 1;
		} else if(auth_algorithm.len == 7
				  && strncmp(auth_algorithm.s, "SHA-512", 7) == 0) {
			algo = 2;
		}
		// For MD5 or empty algorithm, algo remains 0
	}

	// Create ABI-encoded call data for authenticateUser(string,string,string,string,string,uint8,bytes)
	// Function selector: 0xdd02fd8e (calculated from Keccak-256 hash: dd02fd8e9a2d92fbc26f4d9635b61ebdc4e28f3d18e5f2b0c05acbc198507bd9)

	// Convert hex string response to bytes
	int response_byte_len = strlen(response_str) / 2;
	unsigned char response_bytes[64]; // Max 64 bytes for SHA-512
	int actual_byte_len =
			hex_to_bytes(response_str, response_bytes, sizeof(response_bytes));

	if(actual_byte_len != response_byte_len) {
		LM_ERR("Web3Auth: Failed to convert hex response to bytes\n");
		goto cleanup;
	}

	// Calculate string lengths
	size_t len1 = strlen(username_str), len2 = strlen(realm_str),
		   len3 = strlen(method_str);
	size_t len4 = strlen(uri_str), len5 = strlen(nonce_str);

	// Calculate padded lengths (round up to 32-byte boundaries)
	size_t padded_len1 = ((len1 + 31) / 32) * 32;
	size_t padded_len2 = ((len2 + 31) / 32) * 32;
	size_t padded_len3 = ((len3 + 31) / 32) * 32;
	size_t padded_len4 = ((len4 + 31) / 32) * 32;
	size_t padded_len5 = ((len5 + 31) / 32) * 32;
	size_t padded_len7 =
			((actual_byte_len + 31) / 32) * 32; // For response bytes

	// Calculate offsets (selector + 7 offset words = 0xE0 for first string)
	size_t offset1 = 0xE0;
	size_t offset2 = offset1 + 32 + padded_len1;
	size_t offset3 = offset2 + 32 + padded_len2;
	size_t offset4 = offset3 + 32 + padded_len3;
	size_t offset5 = offset4 + 32 + padded_len4;
	size_t offset7 = offset5 + 32 + padded_len5;

	// Calculate total length needed for ABI encoding
	int total_len = 10 + (7 * 64) + (32 + padded_len1) + (32 + padded_len2)
					+ (32 + padded_len3) + (32 + padded_len4)
					+ (32 + padded_len5) + (32 + padded_len7);

	call_data = (char *)pkg_malloc(
			total_len * 2 + 1); // *2 for hex encoding + null terminator
	if(!call_data) {
		LM_ERR("Web3Auth: Failed to allocate memory for ABI data\n");
		goto cleanup;
	}

	int pos = 0;

	// Function selector for authenticateUser
	pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "dd02fd8e");

	// Offset words (32 bytes each, as 64 hex chars)
	pos += snprintf(
			call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset1);
	pos += snprintf(
			call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset2);
	pos += snprintf(
			call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset3);
	pos += snprintf(
			call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset4);
	pos += snprintf(
			call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset5);

	// uint8 algo parameter (padded to 32 bytes)
	pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064x", algo);

	// Offset for response bytes
	pos += snprintf(
			call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset7);

	// String 1: username - length + padded data
	pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", len1);
	for(size_t i = 0; i < len1; i++) {
		pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
				(unsigned char)username_str[i]);
	}
	for(size_t i = len1 * 2; i < padded_len1 * 2; i++) {
		call_data[pos++] = '0';
	}

	// String 2: realm - length + padded data
	pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", len2);
	for(size_t i = 0; i < len2; i++) {
		pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
				(unsigned char)realm_str[i]);
	}
	for(size_t i = len2 * 2; i < padded_len2 * 2; i++) {
		call_data[pos++] = '0';
	}

	// String 3: method - length + padded data
	pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", len3);
	for(size_t i = 0; i < len3; i++) {
		pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
				(unsigned char)method_str[i]);
	}
	for(size_t i = len3 * 2; i < padded_len3 * 2; i++) {
		call_data[pos++] = '0';
	}

	// String 4: uri - length + padded data
	pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", len4);
	for(size_t i = 0; i < len4; i++) {
		pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
				(unsigned char)uri_str[i]);
	}
	for(size_t i = len4 * 2; i < padded_len4 * 2; i++) {
		call_data[pos++] = '0';
	}

	// String 5: nonce - length + padded data
	pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", len5);
	for(size_t i = 0; i < len5; i++) {
		pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
				(unsigned char)nonce_str[i]);
	}
	for(size_t i = len5 * 2; i < padded_len5 * 2; i++) {
		call_data[pos++] = '0';
	}

	// Bytes 7: response - length + padded data
	pos += snprintf(
			call_data + pos, total_len * 2 + 1 - pos, "%064x", actual_byte_len);
	for(int i = 0; i < actual_byte_len; i++) {
		pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
				response_bytes[i]);
	}
	for(size_t i = actual_byte_len * 2; i < padded_len7 * 2; i++) {
		call_data[pos++] = '0';
	}

	call_data[pos] = '\0';

	char payload[32768]; // Increased buffer size for larger call data
	snprintf(payload, sizeof(payload),
			"{\"jsonrpc\":\"2.0\",\"method\":\"eth_call\",\"params\":[{\"to\":"
			"\"%s\",\"data\":\"0x%s\"},\"latest\"],\"id\":1}",
			web3_contract_address, call_data);

	if(web3_debug_mode) {
		const char *algo_name = (algo == 0)	  ? "MD5"
								: (algo == 1) ? "SHA-256"
											  : "SHA-512";
		LM_INFO("Web3Auth: Algorithm: %s (%d)\n", algo_name, algo);
		LM_INFO("Web3Auth: Calling authenticateUser with payload: %s\n",
				payload);
	}

	curl_easy_setopt(curl, CURLOPT_URL, web3_rpc_url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, web3_curl_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &web3_response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)web3_timeout);

	headers = curl_slist_append(NULL, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	res = curl_easy_perform(curl);

	if(res != CURLE_OK) {
		LM_ERR("Web3Auth: curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
		goto cleanup;
	}

	if(!web3_response.memory) {
		LM_ERR("Web3Auth: No response from blockchain\n");
		goto cleanup;
	}

	if(web3_debug_mode) {
		LM_INFO("Web3Auth: Blockchain response: %s\n", web3_response.memory);
	}

	// Parse JSON response to extract the boolean result
	// Looking for: {"jsonrpc":"2.0","id":1,"result":"0x0000000000000000000000000000000000000000000000000000000000000001"}
	// where the last 64 chars represent a uint256 (boolean) - 1 for true, 0 for false
	char *result_start = strstr(web3_response.memory, "\"result\":\"");
	if(!result_start) {
		LM_ERR("Web3Auth: Invalid blockchain response format\n");
		goto cleanup;
	}

	result_start += 10; // Skip "result":"
	char *result_end = strchr(result_start, '"');
	if(!result_end) {
		LM_ERR("Web3Auth: Malformed blockchain response\n");
		goto cleanup;
	}

	// Extract the result (should be 0x followed by 64 hex chars)
	char *hex_start = result_start;
	if(strncmp(hex_start, "0x", 2) == 0) {
		hex_start += 2;
	}

	int hex_len = result_end - hex_start;
	if(hex_len < 64) {
		LM_ERR("Web3Auth: Invalid result length from blockchain: %d (expected "
			   "64)\n",
				hex_len);
		goto cleanup;
	}

	// Check if the last character is '1' (true) or '0' (false)
	char last_char = hex_start[hex_len - 1];
	if(last_char == '1') {
		if(web3_debug_mode) {
			LM_INFO("Web3Auth: Authentication successful! Contract returned "
					"true\n");
		}
		result = AUTHENTICATED;
	} else if(last_char == '0') {
		if(web3_debug_mode) {
			LM_INFO("Web3Auth: Authentication failed! Contract returned "
					"false\n");
		}
		result = NOT_AUTHENTICATED;
	} else {
		LM_ERR("Web3Auth: Invalid boolean result from contract: %c\n",
				last_char);
		result = NOT_AUTHENTICATED;
	}

cleanup:
	if(headers)
		curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	if(web3_response.memory)
		free(web3_response.memory);
	if(call_data)
		pkg_free(call_data);

	return result;
}
/* ===== WEB3 ADDITIONS END ===== */

/*
 * Module destroy function prototype
 */
static void destroy(void);

/*
 * Module initialization function prototype
 */
static int mod_init(void);

/*
 * Remove used credentials from a SIP message header
 */
int w_consume_credentials(struct sip_msg *msg, char *s1, char *s2);
/*
 * Check for credentials with given realm
 */
int w_has_credentials(struct sip_msg *msg, char *s1, char *s2);

static int pv_proxy_authenticate(struct sip_msg *msg, char *realm, char *flags);
static int pv_www_authenticate(struct sip_msg *msg, char *realm, char *flags);
static int pv_www_authenticate2(
		struct sip_msg *msg, char *realm, char *flags, char *method);
static int fixup_pv_auth(void **param, int param_no);
static int w_pv_auth_check(
		sip_msg_t *msg, char *realm, char *flags, char *checks);
static int fixup_pv_auth_check(void **param, int param_no);

static int proxy_challenge(struct sip_msg *msg, char *realm, char *flags);
static int www_challenge(struct sip_msg *msg, char *realm, char *flags);
static int w_auth_challenge(struct sip_msg *msg, char *realm, char *flags);
static int fixup_auth_challenge(void **param, int param_no);

static int w_auth_get_www_authenticate(
		sip_msg_t *msg, char *realm, char *flags, char *dst);
static int fixup_auth_get_www_authenticate(void **param, int param_no);

/*
 * Module parameter variables
 */
char *sec_param =
		0; /* If the parameter was not used, the secret phrase will be auto-generated */
int nonce_expire = 300; /* Nonce lifetime */
/*int   auth_extra_checks = 0;  -- in nonce.c */
int protect_contacts = 0;	   /* Do not include contacts in nonce by default */
int force_stateless_reply = 0; /* Always send reply statelessly */

/*! Prefix to strip from realm */
str auth_realm_prefix = {"", 0};

static int auth_use_domain = 0;

str secret1;
str secret2;
char *sec_rand1 = 0;
char *sec_rand2 = 0;

str challenge_attr = STR_STATIC_INIT("$digest_challenge");
avp_ident_t challenge_avpid;

str proxy_challenge_header = STR_STATIC_INIT("Proxy-Authenticate");
str www_challenge_header = STR_STATIC_INIT("WWW-Authenticate");

struct qp auth_qop = {STR_STATIC_INIT("auth"), QOP_AUTH};

static struct qp auth_qauth = {STR_STATIC_INIT("auth"), QOP_AUTH};

static struct qp auth_qauthint = {STR_STATIC_INIT("auth-int"), QOP_AUTHINT};

/* Hash algorithm used for digest authentication, MD5 if empty */
str auth_algorithm = {"", 0};
int hash_hex_len;
int add_authinfo_hdr =
		0; /* should an Authentication-Info header be added on 200 OK responses? */

calc_HA1_t calc_HA1;
calc_response_t calc_response;


/*! SL API structure */
sl_api_t slb;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
		{"consume_credentials", w_consume_credentials, 0, 0, 0, REQUEST_ROUTE},
		{"www_challenge", (cmd_function)www_challenge, 2, fixup_auth_challenge,
				0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
		{"proxy_challenge", (cmd_function)proxy_challenge, 2,
				fixup_auth_challenge, 0,
				REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
		{"auth_challenge", (cmd_function)w_auth_challenge, 2,
				fixup_auth_challenge, 0,
				REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
		{"pv_www_authorize", (cmd_function)pv_www_authenticate, 2,
				fixup_pv_auth, 0, REQUEST_ROUTE},
		{"pv_www_authenticate", (cmd_function)pv_www_authenticate, 2,
				fixup_pv_auth, 0, REQUEST_ROUTE},
		{"pv_www_authenticate", (cmd_function)pv_www_authenticate2, 3,
				fixup_pv_auth, 0, REQUEST_ROUTE},
		{"pv_proxy_authorize", (cmd_function)pv_proxy_authenticate, 2,
				fixup_pv_auth, 0, REQUEST_ROUTE},
		{"pv_proxy_authenticate", (cmd_function)pv_proxy_authenticate, 2,
				fixup_pv_auth, 0, REQUEST_ROUTE},
		{"auth_get_www_authenticate", (cmd_function)w_auth_get_www_authenticate,
				3, fixup_auth_get_www_authenticate, 0, REQUEST_ROUTE},
		{"has_credentials", w_has_credentials, 1, fixup_spve_null, 0,
				REQUEST_ROUTE},
		{"pv_auth_check", (cmd_function)w_pv_auth_check, 3, fixup_pv_auth_check,
				0, REQUEST_ROUTE},
		{"bind_auth_s", (cmd_function)bind_auth_s, 0, 0, 0},
		{0, 0, 0, 0, 0, 0}};


/*
 * Exported parameters (Enhanced with Web3 parameters)
 */
static param_export_t params[] = {{"secret", PARAM_STRING, &sec_param},
		{"nonce_expire", PARAM_INT, &nonce_expire},
		{"nonce_auth_max_drift", PARAM_INT, &nonce_auth_max_drift},
		{"protect_contacts", PARAM_INT, &protect_contacts},
		{"challenge_attr", PARAM_STR, &challenge_attr},
		{"proxy_challenge_header", PARAM_STR, &proxy_challenge_header},
		{"www_challenge_header", PARAM_STR, &www_challenge_header},
		{"qop", PARAM_STR, &auth_qop.qop_str},
		{"auth_checks_register", PARAM_INT, &auth_checks_reg},
		{"auth_checks_no_dlg", PARAM_INT, &auth_checks_ood},
		{"auth_checks_in_dlg", PARAM_INT, &auth_checks_ind},
		{"nonce_count", PARAM_INT, &nc_enabled},
		{"nc_array_size", PARAM_INT, &nc_array_size},
		{"nc_array_order", PARAM_INT, &nc_array_k},
		{"one_time_nonce", PARAM_INT, &otn_enabled},
		{"otn_in_flight_no", PARAM_INT, &otn_in_flight_no},
		{"otn_in_flight_order", PARAM_INT, &otn_in_flight_k},
		{"nid_pool_no", PARAM_INT, &nid_pool_no},
		{"force_stateless_reply", PARAM_INT, &force_stateless_reply},
		{"realm_prefix", PARAM_STRING, &auth_realm_prefix.s},
		{"use_domain", PARAM_INT, &auth_use_domain},
		{"algorithm", PARAM_STR, &auth_algorithm},
		{"add_authinfo_hdr", INT_PARAM, &add_authinfo_hdr},
		/* === WEB3 PARAMETERS === */
		{"web3_rpc_url", PARAM_STRING, &web3_rpc_url},
		{"web3_contract_address", PARAM_STRING, &web3_contract_address},
		{"web3_debug_mode", PARAM_INT, &web3_debug_mode},
		{"web3_timeout", PARAM_INT, &web3_timeout}, {0, 0, 0}};


/*
 * Module interface
 */
struct module_exports exports = {
		"auth_web3", DEFAULT_DLFLAGS, /* dlopen flags */
		cmds, params, 0,			  /* RPC methods */
		0,							  /* pseudo-variables exports */
		0,							  /* response function */
		mod_init,					  /* module initialization function */
		0,							  /* child initialization function */
		destroy						  /* destroy function */
};


/*
 * Secret parameter was not used so we generate
 * a random value here
 */
static inline int generate_random_secret(void)
{
	int i;

	sec_rand1 = (char *)pkg_malloc(RAND_SECRET_LEN);
	sec_rand2 = (char *)pkg_malloc(RAND_SECRET_LEN);
	if(!sec_rand1 || !sec_rand2) {
		LM_ERR("No memory left\n");
		if(sec_rand1) {
			pkg_free(sec_rand1);
			sec_rand1 = 0;
		}
		return -1;
	}

	for(i = 0; i < RAND_SECRET_LEN; i++) {
		sec_rand1[i] = 32 + (int)(95.0 * kam_rand() / (KAM_RAND_MAX + 1.0));
	}

	secret1.s = sec_rand1;
	secret1.len = RAND_SECRET_LEN;

	for(i = 0; i < RAND_SECRET_LEN; i++) {
		sec_rand2[i] = 32 + (int)(95.0 * kam_rand() / (KAM_RAND_MAX + 1.0));
	}

	secret2.s = sec_rand2;
	secret2.len = RAND_SECRET_LEN;

	/* DBG("Generated secret: '%.*s'\n", secret.len, secret.s); */

	return 0;
}


static int mod_init(void)
{
	str attr;

	DBG("Web3 auth module - initializing\n");

	/* === WEB3 INITIALIZATION === */
	if(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
		LM_ERR("Failed to initialize curl globally\n");
		return -1;
	}

	if(web3_debug_mode) {
		LM_INFO("Web3Auth initialized: RPC_URL=%s, CONTRACT=%s, DEBUG=%d, "
				"TIMEOUT=%d\n",
				web3_rpc_url, web3_contract_address, web3_debug_mode,
				web3_timeout);
	}

	auth_realm_prefix.len = strlen(auth_realm_prefix.s);

	/* bind the SL API */
	if(sl_load_api(&slb) != 0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* If the parameter was not used */
	if(sec_param == 0) {
		/* Generate secret using random generator */
		if(generate_random_secret() < 0) {
			LM_ERR("Error while generating random secret\n");
			return -3;
		}
	} else {
		/* Otherwise use the parameter's value */
		secret1.s = sec_param;
		secret1.len = strlen(secret1.s);

		if(auth_checks_reg || auth_checks_ind || auth_checks_ood) {
			/* divide the secret in half: one half for secret1 and one half for
			 *  secret2 */
			secret2.len = secret1.len / 2;
			secret1.len -= secret2.len;
			secret2.s = secret1.s + secret1.len;
			if(secret2.len < 16) {
				LM_WARN("consider a longer secret when extra auth checks are"
						" enabled (the config secret is divided in 2!)\n");
			}
		}
	}

	if((!challenge_attr.s || challenge_attr.len == 0)
			|| challenge_attr.s[0] != '$') {
		LM_ERR("Invalid value of challenge_attr module parameter\n");
		return -1;
	}

	attr.s = challenge_attr.s + 1;
	attr.len = challenge_attr.len - 1;

	if(parse_avp_ident(&attr, &challenge_avpid) < 0) {
		LM_ERR("Error while parsing value of challenge_attr module"
			   " parameter\n");
		return -1;
	}

	parse_qop(&auth_qop);
	switch(auth_qop.qop_parsed) {
		case QOP_OTHER:
			LM_ERR("Unsupported qop parameter value\n");
			return -1;
		case QOP_AUTH:
		case QOP_AUTHINT:
			if(nc_enabled) {
#ifndef USE_NC
				LM_WARN("nounce count support enabled from config, but"
						" disabled at compile time (recompile with "
						"-DUSE_NC)\n");
				nc_enabled = 0;
#else
				if(nid_crt == 0)
					init_nonce_id();
				if(init_nonce_count() != 0)
					return -1;
#endif
			}
#ifdef USE_NC
			else {
				LM_INFO("qop set, but nonce-count (nonce_count) support"
						" disabled\n");
			}
#endif
			break;
		default:
			if(nc_enabled) {
				LM_WARN("nonce-count support enabled, but qop not set\n");
				nc_enabled = 0;
			}
			break;
	}
	if(otn_enabled) {
#ifdef USE_OT_NONCE
		if(nid_crt == 0)
			init_nonce_id();
		if(init_ot_nonce() != 0)
			return -1;
#else
		LM_WARN("one-time-nonce support enabled from config, but "
				"disabled at compile time (recompile with -DUSE_OT_NONCE)\n");
		otn_enabled = 0;
#endif /* USE_OT_NONCE */
	}

	if(auth_algorithm.len == 0 || strcmp(auth_algorithm.s, "MD5") == 0) {
		hash_hex_len = HASHHEXLEN;
		calc_HA1 = calc_HA1_md5;
		calc_response = calc_response_md5;
	} else if(strcmp(auth_algorithm.s, "SHA-256") == 0) {
		hash_hex_len = HASHHEXLEN_SHA256;
		calc_HA1 = calc_HA1_sha256;
		calc_response = calc_response_sha256;
	} else {
		LM_ERR("Invalid algorithm provided."
			   " Possible values are \"\", \"MD5\" or \"SHA-256\"\n");
		return -1;
	}

	return 0;
}


static void destroy(void)
{
	/* === WEB3 CLEANUP === */
	curl_global_cleanup();

	if(sec_rand1)
		pkg_free(sec_rand1);
	if(sec_rand2)
		pkg_free(sec_rand2);
#ifdef USE_NC
	destroy_nonce_count();
#endif
#ifdef USE_OT_NONCE
	destroy_ot_nonce();
#endif
#if defined USE_NC || defined USE_OT_NONCE
	destroy_nonce_id();
#endif
}


/*
 * Remove used credentials from a SIP message header
 */
int consume_credentials(struct sip_msg *msg)
{
	struct hdr_field *h;
	int len;

	/* skip requests that can't be authenticated */
	if(msg->REQ_METHOD & (METHOD_ACK | METHOD_CANCEL | METHOD_PRACK))
		return -1;
	get_authorized_cred(msg->authorization, &h);
	if(!h) {
		get_authorized_cred(msg->proxy_auth, &h);
		if(!h) {
			LM_ERR("No authorized credentials found (error in scripts)\n");
			return -1;
		}
	}

	len = h->len;

	if(del_lump(msg, h->name.s - msg->buf, len, 0) == 0) {
		LM_ERR("Can't remove credentials\n");
		return -1;
	}

	return 1;
}

/**
 *
 */
int w_consume_credentials(struct sip_msg *msg, char *s1, char *s2)
{
	return consume_credentials(msg);
}

/**
 *
 */
int ki_has_credentials(sip_msg_t *msg, str *srealm)
{
	hdr_field_t *hdr = NULL;
	int ret;

	ret = find_credentials(msg, srealm, HDR_PROXYAUTH_T, &hdr);
	if(ret == 0) {
		LM_DBG("found proxy credentials with realm [%.*s]\n", srealm->len,
				srealm->s);
		return 1;
	}
	ret = find_credentials(msg, srealm, HDR_AUTHORIZATION_T, &hdr);
	if(ret == 0) {
		LM_DBG("found www credentials with realm [%.*s]\n", srealm->len,
				srealm->s);
		return 1;
	}

	LM_DBG("no credentials with realm [%.*s]\n", srealm->len, srealm->s);
	return -1;
}

/**
 *
 */
int w_has_credentials(sip_msg_t *msg, char *realm, char *s2)
{
	str srealm = {0, 0};

	if(fixup_get_svalue(msg, (gparam_t *)realm, &srealm) < 0) {
		LM_ERR("failed to get realm value\n");
		return -1;
	}
	return ki_has_credentials(msg, &srealm);
}

#ifdef USE_NC
/**
 * Calls auth_check_hdr_md5 with the update_nonce flag set to false.
 * Used when flag 32 is set in pv_authenticate.
 */
static int auth_check_hdr_md5_noupdate(
		struct sip_msg *msg, auth_body_t *auth, auth_result_t *auth_res)
{
	return auth_check_hdr_md5(msg, auth, auth_res, 0);
}
#endif

/**
 * @brief do WWW-Digest authentication with password taken from cfg var
 */
int pv_authenticate(struct sip_msg *msg, str *realm, str *passwd, int flags,
		int hftype, str *method)
{
	struct hdr_field *h;
	auth_body_t *cred;
	auth_cfg_result_t ret;
	auth_result_t rauth;
	str hf = {0, 0};
	avp_value_t val;
	static char ha1[256];
	struct qp *qop = NULL;
	check_auth_hdr_t check_auth_hdr = NULL;

	cred = 0;
	ret = AUTH_ERROR;

#ifdef USE_NC
	if(nc_enabled && (flags & 32))
		check_auth_hdr = auth_check_hdr_md5_noupdate;
#endif

	switch(pre_auth(msg, realm, hftype, &h, check_auth_hdr)) {
		case NONCE_REUSED:
			LM_DBG("nonce reused");
			ret = AUTH_NONCE_REUSED;
			goto end;
		case STALE_NONCE:
			LM_DBG("stale nonce\n");
			ret = AUTH_STALE_NONCE;
			goto end;
		case NO_CREDENTIALS:
			LM_DBG("no credentials\n");
			ret = AUTH_NO_CREDENTIALS;
			goto end;
		case ERROR:
		case BAD_CREDENTIALS:
			LM_DBG("error or bad credentials\n");
			ret = AUTH_ERROR;
			goto end;
		case CREATE_CHALLENGE:
			LM_ERR("CREATE_CHALLENGE is not a valid state\n");
			ret = AUTH_ERROR;
			goto end;
		case DO_RESYNCHRONIZATION:
			LM_ERR("DO_RESYNCHRONIZATION is not a valid state\n");
			ret = AUTH_ERROR;
			goto end;
		case NOT_AUTHENTICATED:
			LM_DBG("not authenticated\n");
			ret = AUTH_ERROR;
			goto end;
		case DO_AUTHENTICATION:
			break;
		case AUTHENTICATED:
			ret = AUTH_OK;
			goto end;
	}

	cred = (auth_body_t *)h->parsed;

	/* compute HA1 if needed */
	if((flags & 1) == 0) {
		/* Plaintext password is stored in PV, calculate HA1 */
		calc_HA1(
				HA_MD5, &cred->digest.username.whole, realm, passwd, 0, 0, ha1);
		LM_DBG("HA1 string calculated: %s\n", ha1);
	} else {
		memcpy(ha1, passwd->s, passwd->len);
		ha1[passwd->len] = '\0';
	}

	/* ===== WEB3 REPLACEMENT: The ONLY line that changes! ===== */
	rauth = auth_web3_check_response(&(cred->digest), method, ha1);
	if(rauth == AUTHENTICATED) {
		ret = AUTH_OK;
		switch(post_auth(msg, h, ha1)) {
			case AUTHENTICATED:
				break;
			default:
				ret = AUTH_ERROR;
				break;
		}
	} else {
		if(rauth == NOT_AUTHENTICATED)
			ret = AUTH_INVALID_PASSWORD;
		else
			ret = AUTH_ERROR;
	}

#ifdef USE_NC
	/* On success we need to update the nonce if flag 32 is set */
	if(nc_enabled && ret == AUTH_OK && (flags & 32)) {
		if(check_nonce(cred, &secret1, &secret2, msg, 1) < 0) {
			LM_ERR("check_nonce failed after post_auth");
			ret = AUTH_ERROR;
		}
	}
#endif

end:
	if(ret < 0) {
		/* check if required to add challenge header as avp */
		if(!(flags & 14))
			return ret;
		if(flags & 8) {
			qop = &auth_qauthint;
		} else if(flags & 4) {
			qop = &auth_qauth;
		}
		if(get_challenge_hf(msg, (cred ? cred->stale : 0), realm, NULL,
				   (auth_algorithm.len ? &auth_algorithm : NULL), qop, hftype,
				   &hf)
				< 0) {
			LM_ERR("Error while creating challenge\n");
			ret = AUTH_ERROR;
		} else {
			val.s = hf;
			if(add_avp(challenge_avpid.flags | AVP_VAL_STR,
					   challenge_avpid.name, val)
					< 0) {
				LM_ERR("Error while creating attribute with challenge\n");
				ret = AUTH_ERROR;
			}
			pkg_free(hf.s);
		}
	}

	return ret;
}

/**
 *
 */
static int pv_proxy_authenticate(struct sip_msg *msg, char *realm, char *flags)
{
	int vflags = 0;
	str srealm = {0, 0};
	str spasswd = {0, 0};

	if(get_str_fparam(&srealm, msg, (fparam_t *)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len == 0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if(srealm.len == 0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if(get_int_fparam(&vflags, msg, (fparam_t *)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}
	return pv_authenticate(msg, &srealm, &spasswd, vflags, HDR_PROXYAUTH_T,
			&msg->first_line.u.request.method);

error:
	return AUTH_ERROR;
}

/**
 *
 */
static int pv_www_authenticate(struct sip_msg *msg, char *realm, char *flags)
{
	int vflags = 0;
	str srealm = {0, 0};
	str spasswd = {0, 0};

	if(get_str_fparam(&srealm, msg, (fparam_t *)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len == 0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	// No longer need password input - use dummy value for blockchain auth
	spasswd.s = "dummy";
	spasswd.len = 5;

	if(get_int_fparam(&vflags, msg, (fparam_t *)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}
	return pv_authenticate(msg, &srealm, &spasswd, vflags, HDR_AUTHORIZATION_T,
			&msg->first_line.u.request.method);

error:
	return AUTH_ERROR;
}

static int pv_www_authenticate2(
		struct sip_msg *msg, char *realm, char *flags, char *method)
{
	int vflags = 0;
	str srealm = {0, 0};
	str spasswd = {0, 0};
	str smethod = {0, 0};

	if(get_str_fparam(&srealm, msg, (fparam_t *)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len == 0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	// No longer need password input - use dummy value for blockchain auth
	spasswd.s = "dummy";
	spasswd.len = 5;

	if(get_int_fparam(&vflags, msg, (fparam_t *)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}

	if(get_str_fparam(&smethod, msg, (fparam_t *)method) < 0) {
		LM_ERR("failed to get method value from msg %p var %p\n", msg, method);
		goto error;
	}

	if(smethod.len == 0) {
		LM_ERR("invalid method value - empty content\n");
		goto error;
	}

	return pv_authenticate(
			msg, &srealm, &spasswd, vflags, HDR_AUTHORIZATION_T, &smethod);

error:
	return AUTH_ERROR;
}

/**
 *
 */
static int pv_auth_check(
		sip_msg_t *msg, str *srealm, str *spasswd, int vflags, int vchecks)
{
	int ret;
	hdr_field_t *hdr;
	sip_uri_t *uri = NULL;
	sip_uri_t *turi = NULL;
	sip_uri_t *furi = NULL;
	str suser;

	if(msg->REQ_METHOD == METHOD_REGISTER)
		ret = pv_authenticate(msg, srealm, spasswd, vflags, HDR_AUTHORIZATION_T,
				&msg->first_line.u.request.method);
	else
		ret = pv_authenticate(msg, srealm, spasswd, vflags, HDR_PROXYAUTH_T,
				&msg->first_line.u.request.method);

	if(ret == AUTH_OK && (vchecks & AUTH_CHECK_ID_F)) {
		hdr = (msg->proxy_auth == 0) ? msg->authorization : msg->proxy_auth;
		if(hdr == NULL) {
			if(msg->REQ_METHOD & (METHOD_ACK | METHOD_CANCEL | METHOD_PRACK)) {
				return AUTH_OK;
			} else {
				return AUTH_ERROR;
			}
		}
		suser = ((auth_body_t *)(hdr->parsed))->digest.username.user;

		if((furi = parse_from_uri(msg)) == NULL)
			return AUTH_ERROR;

		if(msg->REQ_METHOD == METHOD_REGISTER
				|| msg->REQ_METHOD == METHOD_PUBLISH) {
			if((turi = parse_to_uri(msg)) == NULL)
				return AUTH_ERROR;
			uri = turi;
		} else {
			uri = furi;
		}
		if(suser.len != uri->user.len
				|| strncmp(suser.s, uri->user.s, suser.len) != 0)
			return AUTH_USER_MISMATCH;

		if(msg->REQ_METHOD == METHOD_REGISTER
				|| msg->REQ_METHOD == METHOD_PUBLISH) {
			/* check from==to */
			if(furi->user.len != turi->user.len
					|| strncmp(furi->user.s, turi->user.s, furi->user.len) != 0)
				return AUTH_USER_MISMATCH;
			if(auth_use_domain != 0
					&& (furi->host.len != turi->host.len
							|| strncmp(furi->host.s, turi->host.s,
									   furi->host.len)
									   != 0))
				return AUTH_USER_MISMATCH;
			/* check r-uri==from for publish */
			if(msg->REQ_METHOD == METHOD_PUBLISH) {
				if(parse_sip_msg_uri(msg) < 0)
					return AUTH_ERROR;
				uri = &msg->parsed_uri;
				if(furi->user.len != uri->user.len
						|| strncmp(furi->user.s, uri->user.s, furi->user.len)
								   != 0)
					return AUTH_USER_MISMATCH;
				if(auth_use_domain != 0
						&& (furi->host.len != uri->host.len
								|| strncmp(furi->host.s, uri->host.s,
										   furi->host.len)
										   != 0))
					return AUTH_USER_MISMATCH;
			}
		}
		return AUTH_OK;
	}

	return ret;
}

/**
 *
 */
static int w_pv_auth_check(
		sip_msg_t *msg, char *realm, char *flags, char *checks)
{
	int vflags = 0;
	int vchecks = 0;
	str srealm = {0, 0};
	str spasswd = {0, 0};


	if(msg == NULL) {
		LM_ERR("invalid msg parameter\n");
		return AUTH_ERROR;
	}

	if((msg->REQ_METHOD == METHOD_ACK) || (msg->REQ_METHOD == METHOD_CANCEL)) {
		return AUTH_OK;
	}

	if(realm == NULL || flags == NULL || checks == NULL) {
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if(get_str_fparam(&srealm, msg, (fparam_t *)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if(srealm.len == 0) {
		LM_ERR("invalid realm value - empty content\n");
		return AUTH_ERROR;
	}

	// No longer need password input - use dummy value for blockchain auth
	spasswd.s = "dummy";
	spasswd.len = 5;

	if(get_int_fparam(&vflags, msg, (fparam_t *)flags) < 0) {
		LM_ERR("invalid flags value\n");
		return AUTH_ERROR;
	}

	if(get_int_fparam(&vchecks, msg, (fparam_t *)checks) < 0) {
		LM_ERR("invalid checks value\n");
		return AUTH_ERROR;
	}
	LM_DBG("realm [%.*s] flags [%d] checks [%d]\n", srealm.len, srealm.s,
			vflags, vchecks);
	return pv_auth_check(msg, &srealm, &spasswd, vflags, vchecks);
}

/**
 * @brief fixup function for pv_auth_check
 */
static int fixup_pv_auth_check(void **param, int param_no)
{
	if(strlen((char *)*param) <= 0) {
		LM_ERR("empty parameter %d not allowed\n", param_no);
		return -1;
	}

	switch(param_no) {
		case 1:
			return fixup_var_pve_str_12(param, 1);
		case 2:
		case 3:
			return fixup_var_int_12(param, 1);
	}
	return 0;
}

/**
 * @brief fixup function for pv_{www,proxy}_authenticate
 */
static int fixup_pv_auth(void **param, int param_no)
{
	if(strlen((char *)*param) <= 0) {
		LM_ERR("empty parameter %d not allowed\n", param_no);
		return -1;
	}

	switch(param_no) {
		case 1:
		case 3:
			return fixup_var_pve_str_12(param, 1);
		case 2:
			return fixup_var_int_12(param, 1);
	}
	return 0;
}

/**
 *
 */
static int auth_send_reply(
		struct sip_msg *msg, int code, char *reason, char *hdr, int hdr_len)
{
	str reason_str;

	/* Add new headers if there are any */
	if((hdr != NULL) && (hdr_len > 0)) {
		if(add_lump_rpl(msg, hdr, hdr_len, LUMP_RPL_HDR) == 0) {
			LM_ERR("failed to append hdr to reply\n");
			return -1;
		}
	}

	reason_str.s = reason;
	reason_str.len = strlen(reason);

	return force_stateless_reply ? slb.sreply(msg, code, &reason_str)
								 : slb.freply(msg, code, &reason_str);
}

/**
 *
 */
int auth_challenge_helper(
		struct sip_msg *msg, str *realm, int flags, int hftype, str *res)
{
	int ret, stale;
	str hf = {0, 0};
	struct qp *qop = NULL;

	ret = -1;

	if(flags & 2) {
		qop = &auth_qauthint;
	} else if(flags & 1) {
		qop = &auth_qauth;
	}
	if(flags & 16) {
		stale = 1;
	} else {
		stale = 0;
	}
	if(get_challenge_hf(msg, stale, realm, NULL,
			   (auth_algorithm.len ? &auth_algorithm : NULL), qop, hftype, &hf)
			< 0) {
		LM_ERR("Error while creating challenge\n");
		ret = -2;
		goto error;
	}

	ret = 1;
	if(res != NULL) {
		*res = hf;
		return ret;
	}
	switch(hftype) {
		case HDR_AUTHORIZATION_T:
			if(auth_send_reply(msg, 401, "Unauthorized", hf.s, hf.len) < 0)
				ret = -3;
			break;
		case HDR_PROXYAUTH_T:
			if(auth_send_reply(
					   msg, 407, "Proxy Authentication Required", hf.s, hf.len)
					< 0)
				ret = -3;
			break;
	}
	if(hf.s)
		pkg_free(hf.s);
	return ret;

error:
	if(hf.s)
		pkg_free(hf.s);
	if(!(flags & 4)) {
		if(auth_send_reply(msg, 500, "Internal Server Error", 0, 0) < 0)
			ret = -4;
	}
	return ret;
}

/**
 *
 */
int auth_challenge_hftype(
		struct sip_msg *msg, str *realm, int flags, int hftype)
{
	return auth_challenge_helper(msg, realm, flags, hftype, NULL);
}

/**
 *
 */
int auth_challenge(sip_msg_t *msg, str *realm, int flags)
{
	int htype;

	if(msg == NULL)
		return -1;

	if(msg->REQ_METHOD == METHOD_REGISTER)
		htype = HDR_AUTHORIZATION_T;
	else
		htype = HDR_PROXYAUTH_T;

	return auth_challenge_helper(msg, realm, flags, htype, NULL);
}

/**
 *
 */
static int proxy_challenge(struct sip_msg *msg, char *realm, char *flags)
{
	int vflags = 0;
	str srealm = {0, 0};

	if(get_str_fparam(&srealm, msg, (fparam_t *)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len == 0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if(get_int_fparam(&vflags, msg, (fparam_t *)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}

	return auth_challenge_hftype(msg, &srealm, vflags, HDR_PROXYAUTH_T);

error:
	if(!(vflags & 4)) {
		if(auth_send_reply(msg, 500, "Internal Server Error", 0, 0) < 0)
			return -4;
	}
	return -1;
}

/**
 *
 */
static int www_challenge(struct sip_msg *msg, char *realm, char *flags)
{
	int vflags = 0;
	str srealm = {0, 0};

	if(get_str_fparam(&srealm, msg, (fparam_t *)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len == 0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if(get_int_fparam(&vflags, msg, (fparam_t *)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}

	return auth_challenge_hftype(msg, &srealm, vflags, HDR_AUTHORIZATION_T);

error:
	if(!(vflags & 4)) {
		if(auth_send_reply(msg, 500, "Internal Server Error", 0, 0) < 0)
			return -4;
	}
	return -1;
}

/**
 *
 */
static int ki_www_challenge(struct sip_msg *msg, str *realm, int flags)
{
	return auth_challenge_hftype(msg, realm, flags, HDR_AUTHORIZATION_T);
}

/**
 *
 */
static int ki_proxy_challenge(struct sip_msg *msg, str *realm, int flags)
{
	return auth_challenge_hftype(msg, realm, flags, HDR_PROXYAUTH_T);
}

/**
 *
 */
static int w_auth_challenge(struct sip_msg *msg, char *realm, char *flags)
{
	int vflags = 0;
	str srealm = {0, 0};

	if((msg->REQ_METHOD == METHOD_ACK) || (msg->REQ_METHOD == METHOD_CANCEL)) {
		return 1;
	}

	if(get_str_fparam(&srealm, msg, (fparam_t *)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len == 0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if(get_int_fparam(&vflags, msg, (fparam_t *)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}

	if(msg->REQ_METHOD == METHOD_REGISTER)
		return auth_challenge_hftype(msg, &srealm, vflags, HDR_AUTHORIZATION_T);
	else
		return auth_challenge_hftype(msg, &srealm, vflags, HDR_PROXYAUTH_T);

error:
	if(!(vflags & 4)) {
		if(auth_send_reply(msg, 500, "Internal Server Error", 0, 0) < 0)
			return -4;
	}
	return -1;
}


/**
 * @brief fixup function for {www,proxy}_challenge
 */
static int fixup_auth_challenge(void **param, int param_no)
{
	if(strlen((char *)*param) <= 0) {
		LM_ERR("empty parameter %d not allowed\n", param_no);
		return -1;
	}

	switch(param_no) {
		case 1:
			return fixup_var_str_12(param, 1);
		case 2:
			return fixup_var_int_12(param, 1);
	}
	return 0;
}


/**
 *
 */
static int w_auth_get_www_authenticate(
		sip_msg_t *msg, char *realm, char *flags, char *dst)
{
	int vflags = 0;
	str srealm = {0};
	str hf = {0};
	pv_spec_t *pv;
	pv_value_t val;
	int ret;

	if(get_str_fparam(&srealm, msg, (fparam_t *)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len == 0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if(get_int_fparam(&vflags, msg, (fparam_t *)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}

	pv = (pv_spec_t *)dst;

	ret = auth_challenge_helper(
			NULL, &srealm, vflags, HDR_AUTHORIZATION_T, &hf);

	if(ret < 0)
		return ret;

	val.rs.s = pv_get_buffer();
	val.rs.len = 0;
	if(hf.s != NULL) {
		memcpy(val.rs.s, hf.s, hf.len);
		val.rs.len = hf.len;
		val.rs.s[val.rs.len] = '\0';
		pkg_free(hf.s);
	}
	val.flags = PV_VAL_STR;
	pv->setf(msg, &pv->pvp, (int)EQ_T, &val);

	return ret;

error:
	return -1;
}


static int fixup_auth_get_www_authenticate(void **param, int param_no)
{
	if(strlen((char *)*param) <= 0) {
		LM_ERR("empty parameter %d not allowed\n", param_no);
		return -1;
	}

	switch(param_no) {
		case 1:
			return fixup_var_str_12(param, 1);
		case 2:
			return fixup_var_int_12(param, 1);
		case 3:
			if(fixup_pvar_null(param, 1) != 0) {
				LM_ERR("failed to fixup result pvar\n");
				return -1;
			}
			if(((pv_spec_t *)(*param))->setf == NULL) {
				LM_ERR("result pvar is not writeble\n");
				return -1;
			}
			return 0;
	}
	return 0;
}

/**
 *
 */
static int ki_auth_get_www_authenticate(
		sip_msg_t *msg, str *realm, int flags, str *pvdst)
{
	str hf = {0};
	pv_spec_t *pvs;
	pv_value_t val;
	int ret;

	pvs = pv_cache_get(pvdst);
	if(pvs == NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", pvdst->len, pvdst->s);
		return -1;
	}

	ret = auth_challenge_helper(NULL, realm, flags, HDR_AUTHORIZATION_T, &hf);

	if(ret < 0)
		return ret;

	val.rs.s = pv_get_buffer();
	val.rs.len = 0;
	if(hf.s != NULL) {
		memcpy(val.rs.s, hf.s, hf.len);
		val.rs.len = hf.len;
		val.rs.s[val.rs.len] = '\0';
		pkg_free(hf.s);
	}
	val.flags = PV_VAL_STR;
	pvs->setf(msg, &pvs->pvp, (int)EQ_T, &val);

	return (ret == 0) ? 1 : ret;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_auth_exports[] = {
	{ str_init("auth"), str_init("consume_credentials"),
		SR_KEMIP_INT, consume_credentials,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth"), str_init("auth_challenge"),
		SR_KEMIP_INT, auth_challenge,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth"), str_init("www_challenge"),
		SR_KEMIP_INT, ki_www_challenge,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth"), str_init("proxy_challenge"),
		SR_KEMIP_INT, ki_proxy_challenge,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth"), str_init("pv_auth_check"),
		SR_KEMIP_INT, pv_auth_check,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth"), str_init("has_credentials"),
		SR_KEMIP_INT, ki_has_credentials,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth"), str_init("auth_get_www_authenticate"),
		SR_KEMIP_INT, ki_auth_get_www_authenticate,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_auth_exports);
	return 0;
}