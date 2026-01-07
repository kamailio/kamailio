/*
 * Arnacon Core Authentication Implementation
 *
 * Copyright (C) 2025 Jonathan Kandel
 */

#include "arnacon_core.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/crypto/sha3.h"
#include "../../core/crypto/shautils.h"

#include <ctype.h>
#include <curl/curl.h>
#include <errno.h>
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Ethereum zero address constant */
#define ARNACON_ETH_ZERO_ADDRESS "0x0000000000000000000000000000000000000000"

/* Internal structure for HTTP response data - prefixed with arnacon_ */
struct arnacon_response_data
{
	char *memory;
	size_t size;
};

/* Internal structure for parsed X-Data - prefixed with arnacon_ */
struct arnacon_xdata
{
	char uuid[ARNACON_MAX_UUID_LENGTH];
	uint64_t timestamp;
};

/* Forward declarations for static functions */
static size_t arnacon_response_write_callback(void *contents, size_t size,
		size_t nmemb, struct arnacon_response_data *response);
static int arnacon_compute_namehash(const char *name, char *hash_hex);
static int arnacon_get_ens_owner(const char *ens_name, char *owner_address,
		const char *registry_address, const char *rpc_url, int debug);
static int arnacon_parse_x_data(
		const char *x_data, struct arnacon_xdata *parsed_data);
static int arnacon_validate_timestamp(uint64_t timestamp, int max_age_seconds);
static int arnacon_recover_ethereum_address(const char *message,
		const char *signature, char *recovered_address, int debug);
static int arnacon_normalize_ethereum_address(
		const char *address, char *normalized);
static int arnacon_is_ethereum_address(const char *str);

/**
 * Initialize the core authentication system
 */
int arnacon_core_init(void)
{
	CURLcode res;

	/* Initialize curl globally */
	res = curl_global_init(CURL_GLOBAL_DEFAULT);
	if(res != CURLE_OK) {
		LM_ERR("Failed to initialize curl: %s\n", curl_easy_strerror(res));
		return -1;
	}

	LM_INFO("arnacon_core: Authentication system initialized\n");
	return 0;
}

/**
 * Cleanup the core authentication system
 */
void arnacon_core_cleanup(void)
{
	curl_global_cleanup();
	LM_INFO("arnacon_core: Authentication system cleaned up\n");
}

/**
 * cURL callback function for collecting response data
 */
static size_t arnacon_response_write_callback(void *contents, size_t size,
		size_t nmemb, struct arnacon_response_data *response)
{
	size_t realsize;
	char *ptr;

	realsize = size * nmemb;

	if(response->memory == NULL) {
		ptr = pkg_malloc(realsize + 1);
	} else {
		ptr = pkg_realloc(response->memory, response->size + realsize + 1);
	}

	if(!ptr) {
		LM_ERR("Failed to allocate memory for response (size: %zu)\n",
				realsize);
		return 0;
	}

	response->memory = ptr;
	memcpy(&(response->memory[response->size]), contents, realsize);
	response->size += realsize;
	response->memory[response->size] = 0;

	return realsize;
}

/**
 * Compute ENS namehash for a given domain name
 */
static int arnacon_compute_namehash(const char *name, char *hash_hex)
{
	unsigned char hash[32] = {0};
	char *name_copy;
	char *labels[64];
	int label_count;
	char *token;
	char *p;
	int i, j;
	sha3_context ctx;
	unsigned char label_hash[32];
	unsigned char combined[64];
	size_t name_len;
	size_t token_len;

	if(!name || !hash_hex) {
		LM_ERR("Invalid input parameters to compute_namehash\n");
		return -1;
	}

	LM_DBG("Computing namehash for: %s\n", name);

	/* Handle empty string (root domain) */
	if(strlen(name) == 0) {
		if(bytes_to_hex(hash, 32, hash_hex, 65) != 0) {
			LM_ERR("Failed to convert root hash to hex\n");
			return -1;
		}
		return 0;
	}

	/* Make a copy of the name for tokenization */
	name_len = strlen(name);
	name_copy = pkg_malloc(name_len + 1);
	if(!name_copy) {
		LM_ERR("Failed to allocate memory for name copy\n");
		return -1;
	}
	memcpy(name_copy, name, name_len);
	name_copy[name_len] = '\0';

	/* Convert to lowercase */
	for(p = name_copy; *p; p++) {
		*p = tolower(*p);
	}

	label_count = 0;

	/* Split by dots */
	token = strtok(name_copy, ".");
	while(token != NULL && label_count < 64) {
		token_len = strlen(token);
		labels[label_count] = pkg_malloc(token_len + 1);
		if(!labels[label_count]) {
			LM_ERR("Failed to allocate memory for label %d\n", label_count);
			/* Cleanup */
			for(j = 0; j < label_count; j++) {
				pkg_free(labels[j]);
			}
			pkg_free(name_copy);
			return -1;
		}
		memcpy(labels[label_count], token, token_len);
		labels[label_count][token_len] = '\0';
		label_count++;
		token = strtok(NULL, ".");
	}

	/* Process labels from right to left (reverse order) */
	for(i = label_count - 1; i >= 0; i--) {
		const void *hash_bytes;
		/* Hash the current label */
		sha3_Init256(&ctx);
		sha3_SetFlags(&ctx, SHA3_FLAGS_KECCAK);
		sha3_Update(&ctx, (const unsigned char *)labels[i], strlen(labels[i]));
		hash_bytes = sha3_Finalize(&ctx);
		memcpy(label_hash, hash_bytes, 32);

		LM_DBG("Label '%s' processed\n", labels[i]);

		/* Combine current hash + label hash */
		memcpy(combined, hash, 32);
		memcpy(combined + 32, label_hash, 32);

		/* Hash the combination */
		sha3_Init256(&ctx);
		sha3_Update(&ctx, combined, 64);
		hash_bytes = sha3_Finalize(&ctx);
		memcpy(hash, hash_bytes, 32);
	}

	/* Convert final hash to hex string */
	if(bytes_to_hex(hash, 32, hash_hex, 65) != 0) {
		LM_ERR("Failed to convert final hash to hex\n");
		/* Cleanup */
		for(i = 0; i < label_count; i++) {
			pkg_free(labels[i]);
		}
		pkg_free(name_copy);
		return -1;
	}

	LM_DBG("Final namehash for '%s': %s\n", name, hash_hex);

	/* Cleanup */
	for(i = 0; i < label_count; i++) {
		pkg_free(labels[i]);
	}
	pkg_free(name_copy);

	return 0;
}

/**
 * Helper function to check if a contract is a NameWrapper by calling its name()
 * function
 */
static int arnacon_is_name_wrapper_contract(
		const char *contract_address, const char *rpc_url, int debug)
{
	CURL *curl;
	CURLcode res;
	struct arnacon_response_data response = {0};
	struct curl_slist *headers;
	int result;
	const char *call_data;
	char payload[1024];
	int ret;
	char *result_start;
	char *result_end;
	size_t result_len;

	headers = NULL;
	result = -1;
	call_data = "0x06fdde03";

	curl = curl_easy_init();
	if(!curl) {
		LM_ERR("Failed to initialize curl for NameWrapper check\n");
		return -1;
	}

	/* name() function selector: 0x06fdde03 */
	ret = snprintf(payload, sizeof(payload),
			"{"
			"\"jsonrpc\":\"2.0\","
			"\"method\":\"eth_call\","
			"\"params\":[{"
			"\"to\":\"%s\","
			"\"data\":\"%s\""
			"},\"latest\"],"
			"\"id\":1"
			"}",
			contract_address, call_data);

	if(ret < 0 || ret >= (int)sizeof(payload)) {
		LM_ERR("snprintf failed or buffer too small for NameWrapper payload\n");
		curl_easy_cleanup(curl);
		return -1;
	}

	if(debug) {
		LM_DBG("Checking if contract is NameWrapper: %s\n", contract_address);
	}

	curl_easy_setopt(curl, CURLOPT_URL, rpc_url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
	curl_easy_setopt(
			curl, CURLOPT_WRITEFUNCTION, arnacon_response_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

	headers = curl_slist_append(NULL, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	res = curl_easy_perform(curl);

	if(res != CURLE_OK) {
		/* Log error always, but include more detail in debug mode */
		LM_ERR("NameWrapper check curl request failed: %s\n",
				curl_easy_strerror(res));
		if(debug) {
			LM_DBG("Contract may not have name() function\n");
		}
		goto cleanup;
	}

	if(!response.memory) {
		LM_ERR("No response from blockchain during NameWrapper check\n");
		goto cleanup;
	}

	if(debug) {
		LM_DBG("NameWrapper check response: %s\n", response.memory);
	}

	/* Parse JSON response to extract the result */
	result_start = strstr(response.memory, "\"result\":\"");
	if(!result_start) {
		if(debug) {
			LM_DBG("Invalid blockchain response format (contract may not have "
				   "name() "
				   "function)\n");
		}
		/* Not an error - contract may just not have name() function */
		result = 0;
		goto cleanup;
	}

	result_start += 10; /* Skip "result":" */
	result_end = strchr(result_start, '"');
	if(!result_end) {
		if(debug) {
			LM_DBG("Malformed blockchain response\n");
		}
		result = 0;
		goto cleanup;
	}

	/* Extract the hex result and decode it */
	result_len = result_end - result_start;
	if(result_len < 64) { /* Need at least 64 chars for offset + length */
		if(debug) {
			LM_DBG("Result too short to contain string data\n");
		}
		result = 0;
		goto cleanup;
	}

	/* Check if we have enough data and if it contains "NameWrapper" */
	if(result_len > 128) {
		/* Convert hex to ASCII and look for "NameWrapper"
     * For simplicity, we'll check if the hex contains the ASCII hex for
     * "NameWrapper" "NameWrapper" in hex is:
     * 4e616d655772617070657200000000000000000000000000000000000000000000 */
		if(strstr(result_start, "4e616d6557726170706572") != NULL) {
			if(debug) {
				LM_DBG("Contract is confirmed to be NameWrapper\n");
			}
			result = 1; /* Is NameWrapper */
		} else {
			if(debug) {
				LM_DBG("Contract name() result does not contain "
					   "'NameWrapper'\n");
			}
			result = 0; /* Not NameWrapper */
		}
	} else {
		if(debug) {
			LM_DBG("String data too short\n");
		}
		result = 0; /* Not NameWrapper */
	}

cleanup:
	if(headers)
		curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	if(response.memory)
		pkg_free(response.memory);

	return result;
}

/**
 * Helper function to make a blockchain RPC call
 */
static int arnacon_make_blockchain_call(const char *contract_address,
		const char *call_data, char *result_address, const char *rpc_url,
		int debug)
{
	CURL *curl;
	CURLcode res;
	struct arnacon_response_data response;
	struct curl_slist *headers;
	int result;
	char payload[1024];
	int ret;
	char *result_start;
	char *result_end;
	size_t result_len;

	memset(&response, 0, sizeof(struct arnacon_response_data));
	headers = NULL;
	result = -1;

	curl = curl_easy_init();
	if(!curl) {
		LM_ERR("Failed to initialize curl for blockchain call\n");
		return -1;
	}

	ret = snprintf(payload, sizeof(payload),
			"{"
			"\"jsonrpc\":\"2.0\","
			"\"method\":\"eth_call\","
			"\"params\":[{"
			"\"to\":\"%s\","
			"\"data\":\"%s\""
			"},\"latest\"],"
			"\"id\":1"
			"}",
			contract_address, call_data);

	if(ret < 0 || ret >= (int)sizeof(payload)) {
		LM_ERR("snprintf failed or buffer too small for blockchain payload\n");
		curl_easy_cleanup(curl);
		return -1;
	}

	if(debug) {
		LM_DBG("Blockchain call to %s\n", contract_address);
		LM_DBG("Call data: %s\n", call_data);
	}

	curl_easy_setopt(curl, CURLOPT_URL, rpc_url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
	curl_easy_setopt(
			curl, CURLOPT_WRITEFUNCTION, arnacon_response_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

	headers = curl_slist_append(NULL, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	res = curl_easy_perform(curl);

	if(res != CURLE_OK) {
		LM_ERR("Blockchain call failed: %s\n", curl_easy_strerror(res));
		goto cleanup;
	}

	if(!response.memory) {
		LM_ERR("No response from blockchain\n");
		goto cleanup;
	}

	if(debug) {
		LM_DBG("Blockchain response: %s\n", response.memory);
	}

	/* Parse JSON response to extract the result */
	result_start = strstr(response.memory, "\"result\":\"");
	if(!result_start) {
		LM_ERR("Invalid blockchain response format (no result field)\n");
		goto cleanup;
	}

	result_start += 10; /* Skip "result":" */
	result_end = strchr(result_start, '"');
	if(!result_end) {
		LM_ERR("Malformed blockchain response (unterminated result)\n");
		goto cleanup;
	}

	/* Extract address (last 40 characters + 0x prefix) */
	result_len = result_end - result_start;
	if(result_len >= 40) {
		ret = snprintf(result_address, ARNACON_MAX_ADDRESS_LENGTH, "0x%s",
				result_start + result_len - 40);
		if(ret < 0 || ret >= ARNACON_MAX_ADDRESS_LENGTH) {
			LM_ERR("snprintf failed or buffer too small for result address\n");
			goto cleanup;
		}
	} else {
		LM_ERR("Invalid result length from blockchain: %zu (expected >= 40)\n",
				result_len);
		goto cleanup;
	}

	result = 0;

cleanup:
	if(headers)
		curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	if(response.memory)
		pkg_free(response.memory);

	return result;
}

/**
 * Make a blockchain RPC call to get ENS owner with dynamic Name Wrapper
 * detection
 */
static int arnacon_get_ens_owner(const char *ens_name, char *owner_address,
		const char *registry_address, const char *rpc_url, int debug)
{
	char namehash_hex[65];
	char call_data[256];
	char registry_owner[ARNACON_MAX_ADDRESS_LENGTH];
	char wrapper_owner[ARNACON_MAX_ADDRESS_LENGTH];
	int is_wrapper;
	int ret;
	size_t addr_len;

	if(!ens_name || !owner_address) {
		LM_ERR("Invalid input parameters to get_ens_owner\n");
		return -1;
	}

	/* Compute namehash */
	if(arnacon_compute_namehash(ens_name, namehash_hex) != 0) {
		LM_ERR("Failed to compute namehash for %s\n", ens_name);
		return -1;
	}

	/* Step 1: Call ENS Registry owner(bytes32) function
   * owner(bytes32) function selector: 0x02571be3 */
	ret = snprintf(call_data, sizeof(call_data), "0x02571be3%s", namehash_hex);
	if(ret < 0 || ret >= (int)sizeof(call_data)) {
		LM_ERR("snprintf failed or buffer too small for owner call\n");
		return -1;
	}

	if(arnacon_make_blockchain_call(
			   registry_address, call_data, registry_owner, rpc_url, debug)
			!= 0) {
		LM_ERR("Failed to call ENS Registry for %s\n", ens_name);
		return -1;
	}

	if(debug) {
		LM_DBG("ENS Registry owner: %s\n", registry_owner);
	}

	/* Check if it's zero address (ENS not found) */
	if(strcmp(registry_owner, ARNACON_ETH_ZERO_ADDRESS) == 0) {
		LM_ERR("ENS name %s not found (zero owner)\n", ens_name);
		return -1;
	}

	/* Step 2: Check if owner is a Name Wrapper contract by calling its name()
   * function */
	if(debug) {
		LM_DBG("Checking if registry owner is a NameWrapper contract: %s\n",
				registry_owner);
	}

	is_wrapper =
			arnacon_is_name_wrapper_contract(registry_owner, rpc_url, debug);

	if(is_wrapper == -1) {
		/* Error occurred during check, but we can still proceed assuming it's not a
     * wrapper */
		if(debug) {
			LM_DBG("Could not determine if owner is NameWrapper (assuming it's "
				   "not), "
				   "returning registry owner: %s\n",
					registry_owner);
		}
		addr_len = strlen(registry_owner);
		memcpy(owner_address, registry_owner, addr_len);
		owner_address[addr_len] = '\0';
		return 0;
	} else if(is_wrapper == 0) {
		/* Owner is NOT a name wrapper (or doesn't have name() function), return
     * registry owner */
		if(debug) {
			LM_DBG("ENS owner is NOT a NameWrapper contract, returning "
				   "registry "
				   "owner: %s\n",
					registry_owner);
		}
		addr_len = strlen(registry_owner);
		memcpy(owner_address, registry_owner, addr_len);
		owner_address[addr_len] = '\0';
		return 0;
	}

	/* Step 3: Owner IS a NameWrapper contract, call its ownerOf(bytes32) function
   */
	if(debug) {
		LM_DBG("ENS owner IS a NameWrapper contract, calling ownerOf() on: "
			   "%s\n",
				registry_owner);
	}

	/* ownerOf(bytes32) function selector: 0x6352211e */
	ret = snprintf(call_data, sizeof(call_data), "0x6352211e%s", namehash_hex);
	if(ret < 0 || ret >= (int)sizeof(call_data)) {
		LM_ERR("snprintf failed or buffer too small for ownerOf call\n");
		return -1;
	}

	if(arnacon_make_blockchain_call(
			   registry_owner, call_data, wrapper_owner, rpc_url, debug)
			!= 0) {
		LM_ERR("Failed to call NameWrapper ownerOf for %s\n", ens_name);
		return -1;
	}

	if(debug) {
		LM_DBG("Name Wrapper NFT owner: %s\n", wrapper_owner);
	}

	/* Check if NFT owner is zero address */
	if(strcmp(wrapper_owner, ARNACON_ETH_ZERO_ADDRESS) == 0) {
		LM_ERR("Name Wrapper returned zero owner for %s\n", ens_name);
		return -1;
	}

	addr_len = strlen(wrapper_owner);
	memcpy(owner_address, wrapper_owner, addr_len);
	owner_address[addr_len] = '\0';
	if(debug) {
		LM_DBG("Final ENS owner (from Name Wrapper): %s\n", owner_address);
	}

	return 0;
}

/**
 * Parse X-Data parameter to extract UUID and timestamp
 */
static int arnacon_parse_x_data(
		const char *x_data, struct arnacon_xdata *parsed_data)
{
	char *colon;
	char *endptr;
	size_t uuid_len;

	if(!x_data || !parsed_data) {
		LM_ERR("Invalid input parameters to parse_x_data\n");
		return -1;
	}

	/* Find the colon separator */
	colon = strchr(x_data, ':');
	if(!colon) {
		LM_ERR("Invalid X-Data format: no colon separator found in '%s'\n",
				x_data);
		return -1;
	}

	/* Extract UUID */
	uuid_len = colon - x_data;
	if(uuid_len >= ARNACON_MAX_UUID_LENGTH) {
		LM_ERR("UUID too long: %zu (max: %d)\n", uuid_len,
				ARNACON_MAX_UUID_LENGTH - 1);
		return -1;
	}

	memcpy(parsed_data->uuid, x_data, uuid_len);
	parsed_data->uuid[uuid_len] = '\0';

	/* Extract timestamp with proper error checking */
	errno = 0;
	endptr = NULL;
	parsed_data->timestamp = strtoull(colon + 1, &endptr, 10);

	/* Check for conversion errors */
	if(errno == ERANGE || errno == EINVAL || endptr == colon + 1
			|| (endptr && *endptr != '\0')) {
		LM_ERR("Invalid timestamp format in X-Data: '%s'\n", colon + 1);
		return -1;
	}

	LM_DBG("Parsed X-Data: UUID=%s, Timestamp=%llu\n", parsed_data->uuid,
			(unsigned long long)parsed_data->timestamp);

	return 0;
}

/**
 * Validate timestamp against current time
 */
static int arnacon_validate_timestamp(uint64_t timestamp, int max_age_seconds)
{
	uint64_t timestamp_seconds;
	uint64_t current_time;

	/* Auto-detect if timestamp is in seconds or milliseconds */
	current_time = (uint64_t)time(NULL);

	if(timestamp > current_time * 100) {
		/* Likely milliseconds, convert to seconds */
		timestamp_seconds = timestamp / 1000;
		LM_DBG("Detected milliseconds timestamp, converted %llu -> %llu\n",
				(unsigned long long)timestamp,
				(unsigned long long)timestamp_seconds);
	} else {
		/* Likely already in seconds */
		timestamp_seconds = timestamp;
		LM_DBG("Detected seconds timestamp: %llu\n",
				(unsigned long long)timestamp_seconds);
	}

	LM_DBG("Validating timestamp: %llu vs current: %llu\n",
			(unsigned long long)timestamp_seconds,
			(unsigned long long)current_time);

	if(current_time > timestamp_seconds
			&& (current_time - timestamp_seconds) > (uint64_t)max_age_seconds) {
		LM_ERR("Timestamp expired: age %llu seconds exceeds max %d seconds\n",
				(unsigned long long)(current_time - timestamp_seconds),
				max_age_seconds);
		return -1;
	}

	LM_DBG("Timestamp validation passed\n");
	return 0;
}

/**
 * Normalize Ethereum address (convert to lowercase and ensure 0x prefix)
 */
static int arnacon_normalize_ethereum_address(
		const char *address, char *normalized)
{
	char *p;
	int ret;
	size_t addr_len;

	if(!address || !normalized) {
		LM_ERR("Invalid input parameters to normalize_ethereum_address\n");
		return -1;
	}

	/* Ensure 0x prefix */
	if(strncmp(address, "0x", 2) == 0 || strncmp(address, "0X", 2) == 0) {
		addr_len = strlen(address);
		if(addr_len >= ARNACON_MAX_ADDRESS_LENGTH) {
			LM_ERR("Address too long: %zu (max: %d)\n", addr_len,
					ARNACON_MAX_ADDRESS_LENGTH - 1);
			return -1;
		}
		memcpy(normalized, address, addr_len);
		normalized[addr_len] = '\0';
	} else {
		ret = snprintf(normalized, ARNACON_MAX_ADDRESS_LENGTH, "0x%s", address);
		if(ret < 0 || ret >= ARNACON_MAX_ADDRESS_LENGTH) {
			LM_ERR("snprintf failed or buffer too small for address "
				   "normalization\n");
			return -1;
		}
	}

	/* Convert to lowercase */
	for(p = normalized; *p; p++) {
		*p = tolower(*p);
	}

	return 0;
}

/**
 * Try to recover address with a specific recovery ID
 */
static int arnacon_try_recover_with_recovery_id(secp256k1_context *ctx,
		const unsigned char *sig_bytes, const unsigned char *message_hash,
		int recovery_id, char *recovered_address, int debug)
{
	secp256k1_ecdsa_recoverable_signature recoverable_sig;
	secp256k1_pubkey pubkey;
	unsigned char pubkey_bytes[65];
	size_t pubkey_len;
	unsigned char addr_hash[32];
	int ret;
	int serialize_result;

	(void)debug; /* Unused parameter */

	/* Create recoverable signature */
	if(!secp256k1_ecdsa_recoverable_signature_parse_compact(
			   ctx, &recoverable_sig, sig_bytes, recovery_id)) {
		LM_DBG("Failed to parse recoverable signature with recovery_id %d\n",
				recovery_id);
		return -1;
	}

	/* Recover public key */
	if(!secp256k1_ecdsa_recover(ctx, &pubkey, &recoverable_sig, message_hash)) {
		LM_DBG("Failed to recover public key with recovery_id %d\n",
				recovery_id);
		return -1;
	}

	/* Serialize public key (uncompressed format) */
	pubkey_len = sizeof(pubkey_bytes);
	serialize_result = secp256k1_ec_pubkey_serialize(
			ctx, pubkey_bytes, &pubkey_len, &pubkey, SECP256K1_EC_UNCOMPRESSED);
	if(!serialize_result || pubkey_len != 65) {
		LM_ERR("Failed to serialize public key (result=%d, len=%zu)\n",
				serialize_result, pubkey_len);
		return -1;
	}

	/* Ethereum address is the last 20 bytes of the Keccak hash of the public key
   * (excluding the first byte) */
	sha3_HashBuffer(256, SHA3_FLAGS_KECCAK, pubkey_bytes + 1, 64, addr_hash,
			sizeof(addr_hash));

	/* Convert last 20 bytes to hex address */
	ret = snprintf(recovered_address, ARNACON_MAX_ADDRESS_LENGTH, "0x");
	if(ret < 0 || ret >= ARNACON_MAX_ADDRESS_LENGTH) {
		LM_ERR("snprintf failed for address prefix\n");
		return -1;
	}
	/* Buffer size for hex: 20 bytes * 2 + 1 null = 41, we have 43-2 = 41 available */
	if(bytes_to_hex(addr_hash + 12, 20, recovered_address + 2, 41) != 0) {
		LM_ERR("Failed to convert address hash to hex\n");
		return -1;
	}

	return 0;
}

/**
 * Recover Ethereum address from ECDSA signature
 */
static int arnacon_recover_ethereum_address(const char *message,
		const char *signature, char *recovered_address, int debug)
{
	secp256k1_context *ctx;
	unsigned char sig_bytes[65];
	int sig_len;
	char prefixed_message[512];
	char ethereum_prefix;
	int prefix_len;
	unsigned char message_hash[32];
	int v_value;
	int primary_recovery_id;
	int recovery_id;
	char temp_address[ARNACON_MAX_ADDRESS_LENGTH];
	size_t addr_len;

	ethereum_prefix = 0x19;

	if(!message || !signature || !recovered_address) {
		LM_ERR("Invalid input parameters to recover_ethereum_address\n");
		return -1;
	}

	if(debug) {
		LM_DBG("Recovering address from message: %s\n", message);
		LM_DBG("Signature: %s\n", signature);
	}

	/* Initialize secp256k1 context */
	ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
	if(!ctx) {
		LM_ERR("Failed to create secp256k1 context\n");
		return -1;
	}

	/* Parse signature (65 bytes: r + s + v) */
	sig_len = hex_to_bytes(signature, sig_bytes, sizeof(sig_bytes));
	if(sig_len != 65) {
		LM_ERR("Invalid signature length: %d (expected 65 bytes)\n", sig_len);
		secp256k1_context_destroy(ctx);
		return -1;
	}

	/* Hash the message with Ethereum's message prefix */
	prefix_len = snprintf(prefixed_message, sizeof(prefixed_message),
			"%cEthereum Signed Message:\n%zu%s", ethereum_prefix,
			strlen(message), message);

	if(prefix_len < 0 || prefix_len >= (int)sizeof(prefixed_message)) {
		LM_ERR("snprintf failed or buffer too small for prefixed message\n");
		secp256k1_context_destroy(ctx);
		return -1;
	}

	sha3_HashBuffer(256, SHA3_FLAGS_KECCAK, (unsigned char *)prefixed_message,
			prefix_len, message_hash, sizeof(message_hash));

	if(debug) {
		LM_DBG("Message hash computed\n");
	}

	/* Extract v value (last byte) */
	v_value = sig_bytes[64];
	if(debug) {
		LM_DBG("v value: 0x%02x (%d)\n", v_value, v_value);
	}

	/* Calculate recovery ID from v value */
	if(v_value >= 27 && v_value <= 28) {
		primary_recovery_id = v_value - 27;
	} else if(v_value >= 0 && v_value <= 3) {
		primary_recovery_id = v_value;
	} else {
		LM_WARN("Unusual v value %d, will try both recovery IDs\n", v_value);
		primary_recovery_id = -1; /* Invalid, will try both */
	}

	/* Try the primary recovery ID first (if valid) */
	if(primary_recovery_id >= 0 && primary_recovery_id <= 1) {
		if(debug) {
			LM_DBG("Trying primary recovery_id: %d\n", primary_recovery_id);
		}
		if(arnacon_try_recover_with_recovery_id(ctx, sig_bytes, message_hash,
				   primary_recovery_id, recovered_address, debug)
				== 0) {
			if(debug) {
				LM_DBG("Successfully recovered with primary recovery_id %d: "
					   "%s\n",
						primary_recovery_id, recovered_address);
			}
			secp256k1_context_destroy(ctx);
			return 0;
		}
		if(debug) {
			LM_DBG("Primary recovery_id %d failed, trying fallback\n",
					primary_recovery_id);
		}
	}

	/* Fallback: try both recovery IDs (0 and 1) */
	if(debug) {
		LM_DBG("Trying fallback recovery with both IDs...\n");
	}
	for(recovery_id = 0; recovery_id <= 1; recovery_id++) {
		if(recovery_id == primary_recovery_id) {
			continue; /* Already tried this one */
		}

		if(debug) {
			LM_DBG("Trying fallback recovery_id: %d\n", recovery_id);
		}
		if(arnacon_try_recover_with_recovery_id(ctx, sig_bytes, message_hash,
				   recovery_id, temp_address, debug)
				== 0) {
			if(debug) {
				LM_DBG("Successfully recovered with fallback recovery_id %d: "
					   "%s\n",
						recovery_id, temp_address);
			}
			addr_len = strlen(temp_address);
			memcpy(recovered_address, temp_address, addr_len);
			recovered_address[addr_len] = '\0';
			secp256k1_context_destroy(ctx);
			return 0;
		}
	}

	LM_ERR("Failed to recover Ethereum address with any recovery ID\n");
	secp256k1_context_destroy(ctx);
	return -1;
}

/**
 * Check if a string is a valid Ethereum address (0x + 40 hex chars)
 * @return 1 if valid address, 0 if not
 */
static int arnacon_is_ethereum_address(const char *str)
{
	int i;
	size_t len;

	if(!str) {
		LM_DBG("is_ethereum_address: input is NULL\n");
		return 0;
	}

	/* Check for 0x prefix */
	if(strncmp(str, "0x", 2) != 0 && strncmp(str, "0X", 2) != 0) {
		LM_DBG("is_ethereum_address: missing 0x prefix in '%s'\n", str);
		return 0;
	}

	/* Check length (0x + 40 hex chars = 42) */
	len = strlen(str);
	if(len != 42) {
		LM_DBG("is_ethereum_address: invalid length %zu (expected 42) for "
			   "'%s'\n",
				len, str);
		return 0;
	}

	/* Check if all characters after 0x are hex digits */
	for(i = 2; i < 42; i++) {
		if(!((str[i] >= '0' && str[i] <= '9')
				   || (str[i] >= 'a' && str[i] <= 'f')
				   || (str[i] >= 'A' && str[i] <= 'F'))) {
			LM_DBG("is_ethereum_address: invalid hex character '%c' at "
				   "position %d\n",
					str[i], i);
			return 0;
		}
	}

	return 1;
}

/**
 * Main authentication function
 * Supports both ENS names and plain Ethereum addresses
 */
int arnacon_core_authenticate(const char *ens, const char *x_data,
		const char *x_sign, const char *registry_address, const char *rpc_url,
		int timeout_seconds, int debug)
{
	struct arnacon_xdata parsed_data;
	char ens_owner_address[ARNACON_MAX_ADDRESS_LENGTH];
	char recovered_address[ARNACON_MAX_ADDRESS_LENGTH];
	char normalized_ens_address[ARNACON_MAX_ADDRESS_LENGTH];
	char normalized_recovered_address[ARNACON_MAX_ADDRESS_LENGTH];
	int is_plain_address;

	if(!ens || !x_data || !x_sign) {
		LM_ERR("Invalid input parameters to authenticate\n");
		return ARNACON_ERROR;
	}

	if(debug) {
		LM_DBG("=== Authentication ===\n");
		LM_DBG("User identifier: %s\n", ens);
		LM_DBG("X-Data: %s\n", x_data);
		LM_DBG("X-Sign: %s\n", x_sign);
	}

	/* Step 1: Parse and validate X-Data */
	if(arnacon_parse_x_data(x_data, &parsed_data) != 0) {
		LM_ERR("Authentication failed: Invalid X-Data format\n");
		return ARNACON_FAILURE;
	}

	/* Step 2: Validate timestamp */
	if(arnacon_validate_timestamp(parsed_data.timestamp, timeout_seconds)
			!= 0) {
		LM_ERR("Authentication failed: Signature timestamp expired\n");
		return ARNACON_FAILURE;
	}

	/* Step 3: Determine if input is an Ethereum address or ENS name */
	is_plain_address = arnacon_is_ethereum_address(ens);

	if(is_plain_address) {
		/* Plain Ethereum address - use directly */
		if(debug) {
			LM_DBG("Input is a plain Ethereum address\n");
		}
		strncpy(ens_owner_address, ens, ARNACON_MAX_ADDRESS_LENGTH - 1);
		ens_owner_address[ARNACON_MAX_ADDRESS_LENGTH - 1] = '\0';
	} else {
		/* ENS name - resolve to owner address */
		if(debug) {
			LM_DBG("Input is an ENS name, resolving...\n");
		}
		if(arnacon_get_ens_owner(
				   ens, ens_owner_address, registry_address, rpc_url, debug)
				!= 0) {
			LM_ERR("Authentication failed: Could not resolve ENS owner for "
				   "%s\n",
					ens);
			return ARNACON_FAILURE;
		}
	}

	/* Step 4: Recover signer address from signature */
	if(arnacon_recover_ethereum_address(
			   x_data, x_sign, recovered_address, debug)
			!= 0) {
		LM_ERR("Authentication failed: Could not recover address from "
			   "signature\n");
		return ARNACON_FAILURE;
	}

	/* Step 5: Normalize addresses for comparison */
	if(arnacon_normalize_ethereum_address(
			   ens_owner_address, normalized_ens_address)
					!= 0
			|| arnacon_normalize_ethereum_address(
					   recovered_address, normalized_recovered_address)
					   != 0) {
		LM_ERR("Authentication failed: Address normalization error\n");
		return ARNACON_ERROR;
	}

	/* Step 6: Compare addresses */
	if(debug) {
		LM_DBG("=== Address Comparison ===\n");
		LM_DBG("ENS owner address:   %s\n", normalized_ens_address);
		LM_DBG("Recovered address:   %s\n", normalized_recovered_address);
	}

	if(strcmp(normalized_ens_address, normalized_recovered_address) == 0) {
		if(debug) {
			LM_DBG("Authentication SUCCESS: Addresses match!\n");
		}
		return ARNACON_SUCCESS;
	} else {
		/* Log authentication failure as error - addresses don't match */
		LM_ERR("Authentication FAILED: Addresses do not match "
			   "(expected: %s, got: %s)\n",
				normalized_ens_address, normalized_recovered_address);
		return ARNACON_FAILURE;
	}
}

/**
 * Check if user exists (ENS has an owner or address is valid)
 * Supports both ENS names and plain Ethereum addresses
 */
int arnacon_core_user_exists(const char *ens, const char *registry_address,
		const char *rpc_url, int debug)
{
	char owner_address[ARNACON_MAX_ADDRESS_LENGTH];
	int result;
	int is_plain_address;

	if(!ens) {
		LM_ERR("Invalid user identifier parameter (NULL)\n");
		return -1;
	}

	if(debug) {
		LM_DBG("Checking if user exists: %s\n", ens);
	}

	/* Check if input is a plain Ethereum address */
	is_plain_address = arnacon_is_ethereum_address(ens);

	if(is_plain_address) {
		/* Plain Ethereum address - always exists if valid format */
		if(debug) {
			LM_DBG("User identifier is a valid Ethereum address: %s\n", ens);
		}
		return 1; /* User exists (valid address) */
	}

	/* ENS name - try to resolve owner */
	result = arnacon_get_ens_owner(
			ens, owner_address, registry_address, rpc_url, debug);

	if(result == 0) {
		/* Successfully got an owner */
		if(debug) {
			LM_DBG("ENS exists: %s -> %s\n", ens, owner_address);
		}
		return 1; /* User exists */
	} else {
		/* Failed to get owner (ENS not found or error) */
		if(debug) {
			LM_DBG("ENS does not exist: %s\n", ens);
		}
		return 0; /* User does not exist */
	}
}
