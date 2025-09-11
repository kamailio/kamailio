/*
 * Web3 Authentication - Core Authentication Implementation
 *
 * Copyright (C) 2025 Jonathan Kandel
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "web3_imple.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"
#include "../../modules/auth/api.h"
#include "auth_web3_mod.h"
#include "keccak256.h"
#include <curl/curl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Convert hex string to bytes
 */
int hex_to_bytes(const char *hex_str, unsigned char *bytes, int max_bytes) {
  int len = strlen(hex_str);
  if (len % 2 != 0)
    return -1; // Invalid hex string

  int byte_len = len / 2;
  if (byte_len > max_bytes)
    return -1; // Too many bytes

  for (int i = 0; i < byte_len; i++) {
    char hex_byte[3] = {hex_str[i * 2], hex_str[i * 2 + 1], '\0'};
    bytes[i] = (unsigned char)strtol(hex_byte, NULL, 16);
  }

  return byte_len;
}

/**
 * Curl callback function for Web3 RPC responses
 */
size_t web3_curl_callback(void *contents, size_t size, size_t nmemb,
                          struct Web3ResponseData *data) {
  size_t realsize = size * nmemb;
  char *ptr = realloc(data->memory, data->size + realsize + 1);
  if (!ptr)
    return 0;

  data->memory = ptr;
  memcpy(&(data->memory[data->size]), contents, realsize);
  data->size += realsize;
  data->memory[data->size] = 0;
  return realsize;
}

/**
 * Helper function to make blockchain calls for ENS and Oasis queries
 * Now accepts rpc_url parameter to support different networks
 */
static int web3_blockchain_call(const char *rpc_url, const char *to_address,
                                const char *data, char *result_buffer,
                                size_t buffer_size) {
  CURL *curl;
  CURLcode res;
  struct Web3ResponseData web3_response = {0};
  struct curl_slist *headers = NULL;
  int result = -1;

  curl = curl_easy_init();
  if (!curl) {
    LM_ERR("Web3Auth: Failed to initialize curl for blockchain call\n");
    return -1;
  }

  char payload[4096];
  snprintf(payload, sizeof(payload),
           "{\"jsonrpc\":\"2.0\",\"method\":\"eth_call\",\"params\":[{\"to\":"
           "\"%s\",\"data\":\"%s\"},\"latest\"],\"id\":1}",
           to_address, data);

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Blockchain call to %s using RPC: %s\n", to_address,
            rpc_url);
    LM_INFO("Web3Auth: Call data: %s\n", data);
  }

  curl_easy_setopt(curl, CURLOPT_URL, rpc_url);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, web3_curl_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &web3_response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)rpc_timeout);

  headers = curl_slist_append(NULL, "Content-Type: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    LM_ERR("Web3Auth: curl_easy_perform() failed: %s\n",
           curl_easy_strerror(res));
    goto cleanup;
  }

  if (!web3_response.memory) {
    LM_ERR("Web3Auth: No response from blockchain\n");
    goto cleanup;
  }

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Blockchain response: %s\n", web3_response.memory);
  }

  // Parse JSON response to extract the result
  char *result_start = strstr(web3_response.memory, "\"result\":\"");
  if (!result_start) {
    // Check if it's an error response
    char *error_start = strstr(web3_response.memory, "\"error\":");
    if (error_start) {
      // Check for specific error messages
      char *message_start = strstr(web3_response.memory, "\"message\":");
      if (message_start && strstr(message_start, "User not found")) {
        if (contract_debug_mode) {
          LM_INFO("Web3Auth: Contract returned 'User not found' - treating as "
                  "zero address\n");
        }
        strcpy(result_buffer, "0x0000000000000000000000000000000000000000");
        result = 0;
        goto cleanup;
      }
    }
    LM_ERR("Web3Auth: Invalid blockchain response format\n");
    goto cleanup;
  }

  result_start += 10; // Skip "result":"
  char *result_end = strchr(result_start, '"');
  if (!result_end) {
    LM_ERR("Web3Auth: Malformed blockchain response\n");
    goto cleanup;
  }

  // Copy result to buffer
  size_t result_len = result_end - result_start;
  if (result_len >= buffer_size) {
    LM_ERR("Web3Auth: Result too long for buffer\n");
    goto cleanup;
  }

  memcpy(result_buffer, result_start, result_len);
  result_buffer[result_len] = '\0';
  result = 0;

cleanup:
  if (headers)
    curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  if (web3_response.memory)
    free(web3_response.memory);

  return result;
}

// Convert bytes to hex string
static void bytes_to_hex(const unsigned char *bytes, size_t len, char *hex) {
  for (size_t i = 0; i < len; i++) {
    sprintf(hex + 2 * i, "%02x", bytes[i]);
  }
  hex[2 * len] = '\0';
}

// ENS namehash implementation using proper keccak256
static void ens_namehash(const char *name, char *hash_hex) {
  unsigned char hash[32] = {0}; // Start with 32 zero bytes

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Computing namehash for: %s\n", name);
  }

  // Handle empty string (root domain)
  if (strlen(name) == 0) {
    bytes_to_hex(hash, 32, hash_hex);
    return;
  }

  // Split domain into labels and process from right to left
  char *name_copy = pkg_malloc(strlen(name) + 1);
  if (!name_copy) {
    LM_ERR("Web3Auth: Failed to allocate memory for name copy\n");
    memset(hash_hex, '0', 64);
    hash_hex[64] = '\0';
    return;
  }
  strcpy(name_copy, name);

  char *labels[64]; // Max 64 labels should be enough
  int label_count = 0;

  // Split by dots
  char *token = strtok(name_copy, ".");
  while (token != NULL && label_count < 64) {
    size_t token_len = strlen(token);
    labels[label_count] = pkg_malloc(token_len + 1);
    if (!labels[label_count]) {
      LM_ERR("Web3Auth: Failed to allocate memory for label %d\n", label_count);
      // Cleanup already allocated labels
      for (int j = 0; j < label_count; j++) {
        pkg_free(labels[j]);
      }
      pkg_free(name_copy);
      memset(hash_hex, '0', 64);
      hash_hex[64] = '\0';
      return;
    }
    strcpy(labels[label_count], token);
    label_count++;
    token = strtok(NULL, ".");
  }

  // Process labels from right to left (reverse order)
  for (int i = label_count - 1; i >= 0; i--) {
    SHA3_CTX ctx;
    unsigned char label_hash[32];
    unsigned char combined[64]; // hash + label_hash

    // Hash the current label
    keccak_init(&ctx);
    keccak_update(&ctx, (const unsigned char *)labels[i], strlen(labels[i]));
    keccak_final(&ctx, label_hash);

    if (contract_debug_mode) {
      char label_hash_hex[65];
      bytes_to_hex(label_hash, 32, label_hash_hex);
      LM_INFO("Web3Auth: Label '%s' hash: %s\n", labels[i], label_hash_hex);
    }

    // Combine current hash + label hash
    memcpy(combined, hash, 32);
    memcpy(combined + 32, label_hash, 32);

    // Hash the combination
    keccak_init(&ctx);
    keccak_update(&ctx, combined, 64);
    keccak_final(&ctx, hash);

    if (contract_debug_mode) {
      char current_hash_hex[65];
      bytes_to_hex(hash, 32, current_hash_hex);
      LM_INFO("Web3Auth: After processing '%s': %s\n", labels[i],
              current_hash_hex);
    }
  }

  // Convert final hash to hex string
  bytes_to_hex(hash, 32, hash_hex);

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Final namehash for '%s': %s\n", name, hash_hex);
  }

  // Cleanup
  for (int i = 0; i < label_count; i++) {
    pkg_free(labels[i]);
  }
  pkg_free(name_copy);
}

/**
 * Get ENS owner address using the new approach:
 * 1. Call ENS Registry owner(bytes32) function with namehash
 * 2. If zero address, return ENS not found
 * 3. If owner != name wrapper address, return that owner
 * 4. If owner == name wrapper address, call name wrapper ownerOf(bytes32)
 */
int web3_ens_get_owner_address(const char *ens_name, char *owner_address) {
  char namehash_hex[65];
  char call_data[256];
  char result[256];

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Getting ENS owner address for: %s\n", ens_name);
  }

  // Step 1: Get namehash
  ens_namehash(ens_name, namehash_hex);

  // Step 2: Call ENS Registry owner(bytes32) function
  // owner(bytes32) function selector: 0x02571be3
  snprintf(call_data, sizeof(call_data), "0x02571be3%s", namehash_hex);

  const char *rpc_url = ens_rpc_url ? ens_rpc_url : authentication_rpc_url;
  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Calling ENS Registry owner() with data: %s\n",
            call_data);
    LM_INFO("Web3Auth: Using ENS RPC: %s\n", rpc_url);
    LM_INFO("Web3Auth: ENS Registry Address: %s\n", ens_registry_address);
  }

  if (web3_blockchain_call(rpc_url, ens_registry_address, call_data, result,
                           sizeof(result)) != 0) {
    LM_ERR("Web3Auth: Failed to call ENS Registry owner function\n");
    return -1;
  }

  // Extract owner address (last 40 characters)
  char registry_owner[43] = {0};
  if (strlen(result) >= 40) {
    snprintf(registry_owner, 43, "0x%s", result + strlen(result) - 40);
  } else {
    LM_ERR("Web3Auth: Invalid ENS Registry response format\n");
    return -1;
  }

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: ENS Registry owner: %s\n", registry_owner);
  }

  // Step 3: Check if owner is zero address (ENS not found)
  if (strcmp(registry_owner, "0x0000000000000000000000000000000000000000") ==
      0) {
    LM_ERR("Web3Auth: ENS name %s not found (zero owner)\n", ens_name);
    strcpy(owner_address, "0x0000000000000000000000000000000000000000");
    return 1; // Special return code for ENS not found
  }

  // Step 4: Compare with name wrapper address
  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Comparing registry owner with name wrapper address:\n");
    LM_INFO("Web3Auth:   Registry owner: %s\n", registry_owner);
    LM_INFO("Web3Auth:   Name wrapper:   %s\n", ens_name_wrapper_address);
  }

  if (strcasecmp(registry_owner, ens_name_wrapper_address) != 0) {
    // Owner is NOT the name wrapper, return registry owner
    strcpy(owner_address, registry_owner);
    if (contract_debug_mode) {
      LM_INFO("Web3Auth: ENS owner is NOT name wrapper, returning registry "
              "owner: %s\n",
              owner_address);
    }
    return 0;
  }

  // Step 5: Owner IS the name wrapper, call name wrapper ownerOf(bytes32)
  if (contract_debug_mode) {
    LM_INFO("Web3Auth: ENS owner IS name wrapper, calling name wrapper "
            "ownerOf()\n");
  }

  // ownerOf(bytes32) function selector: 0x6352211e
  snprintf(call_data, sizeof(call_data), "0x6352211e%s", namehash_hex);

  if (web3_blockchain_call(rpc_url, ens_name_wrapper_address, call_data, result,
                           sizeof(result)) != 0) {
    LM_ERR("Web3Auth: Failed to call Name Wrapper ownerOf function\n");
    return -1;
  }

  // Extract NFT owner address (last 40 characters)
  if (strlen(result) >= 40) {
    snprintf(owner_address, 43, "0x%s", result + strlen(result) - 40);
  } else {
    LM_ERR("Web3Auth: Invalid Name Wrapper response format\n");
    return -1;
  }

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Name Wrapper NFT owner: %s\n", owner_address);
  }

  // Check if NFT owner is zero address
  if (strcmp(owner_address, "0x0000000000000000000000000000000000000000") ==
      0) {
    LM_ERR("Web3Auth: Name Wrapper returned zero owner for %s\n", ens_name);
    return 1; // ENS not found
  }

  return 0;
}

/**
 * Legacy function name for compatibility - now calls the new owner resolution
 * This maintains compatibility with existing code that calls
 * web3_ens_resolve_address
 */
int web3_ens_resolve_address(const char *ens_name, char *resolved_address) {
  return web3_ens_get_owner_address(ens_name, resolved_address);
}

/**
 * Get wallet address from Oasis contract using getWalletAddress function
 */
int web3_oasis_get_wallet_address(const char *username, char *wallet_address) {
  char call_data[512];
  char result[256];
  int pos = 0;

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Getting Oasis wallet address for username: %s\n",
            username);
  }

  // Function selector for getWalletAddress(string) - found by testing: 08f20630
  pos += snprintf(call_data + pos, sizeof(call_data) - pos, "08f20630");

  // Offset to data (32 bytes from start)
  pos += snprintf(
      call_data + pos, sizeof(call_data) - pos,
      "0000000000000000000000000000000000000000000000000000000000000020");

  // String length (username length in bytes)
  size_t username_len = strlen(username);
  pos += snprintf(call_data + pos, sizeof(call_data) - pos, "%064lx",
                  username_len);

  // String data (username in hex, padded to 32-byte boundary)
  for (size_t i = 0; i < username_len; i++) {
    pos += snprintf(call_data + pos, sizeof(call_data) - pos, "%02x",
                    (unsigned char)username[i]);
  }

  // Pad to 32-byte boundary
  size_t padding = (32 - (username_len % 32)) % 32;
  for (size_t i = 0; i < padding; i++) {
    pos += snprintf(call_data + pos, sizeof(call_data) - pos, "00");
  }

  // Prepend 0x
  char final_call_data[1024];
  snprintf(final_call_data, sizeof(final_call_data), "0x%s", call_data);

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Using main RPC for Oasis query: %s\n",
            authentication_rpc_url);
  }

  if (web3_blockchain_call(authentication_rpc_url,
                           authentication_contract_address, final_call_data,
                           result, sizeof(result)) != 0) {
    LM_ERR("Web3Auth: Failed to call Oasis contract\n");
    return -1;
  }

  // Extract address from result (last 40 hex chars of the 64-char response)
  if (strlen(result) >= 40) {
    snprintf(wallet_address, 43, "0x%s", result + strlen(result) - 40);
  } else {
    LM_ERR("Web3Auth: Invalid Oasis response format\n");
    return -1;
  }

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Oasis wallet address for %s: %s\n", username,
            wallet_address);
  }

  return 0;
}

/**
 * Check if username is ENS format and validate against Oasis contract
 * Now uses ENS owner resolution instead of address resolution
 */
int web3_ens_validate(const char *username, dig_cred_t *cred, str *method) {
  // Check if username contains "." (ENS format)
  if (!strchr(username, '.')) {
    // Not an ENS name, proceed with normal authentication
    return auth_web3_check_response(cred, method);
  }

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Detected ENS name: %s\n", username);
  }

  char ens_owner_address[43] = {0};
  char oasis_wallet_address[43] = {0};

  // Extract auth username from credentials for Oasis contract
  char auth_username[256];
  if (cred->username.user.len >= sizeof(auth_username)) {
    LM_ERR("Web3Auth: Auth username too long (%d chars)\n",
           cred->username.user.len);
    return NOT_AUTHENTICATED;
  }
  memcpy(auth_username, cred->username.user.s, cred->username.user.len);
  auth_username[cred->username.user.len] = '\0';

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: ENS name (from From field): %s\n", username);
    LM_INFO("Web3Auth: Auth username (for Oasis): %s\n", auth_username);
  }

  // Step 1: Get ENS owner address (new approach)
  int ens_result = web3_ens_get_owner_address(username, ens_owner_address);
  if (ens_result == 1) {
    LM_ERR("Web3Auth: ENS name %s not found or has zero owner\n", username);
    return 402; // ENS not valid
  } else if (ens_result != 0) {
    LM_ERR("Web3Auth: Failed to get ENS owner address for %s\n", username);
    return 402; // ENS not valid
  }

  // Step 2: Get wallet address from Oasis contract (use auth username)
  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Calling Oasis contract with auth username: %s\n",
            auth_username);
  }
  if (web3_oasis_get_wallet_address(auth_username, oasis_wallet_address) != 0) {
    LM_ERR("Web3Auth: Failed to get wallet address from Oasis for %s\n",
           auth_username);
    return NOT_AUTHENTICATED;
  }

  // Step 3: Compare owner addresses
  if (contract_debug_mode) {
    LM_INFO("Web3Auth: ENS owner address: %s (owner of ENS name %s)\n",
            ens_owner_address, username);
    LM_INFO("Web3Auth: Oasis wallet address: %s (from Oasis contract for %s)\n",
            oasis_wallet_address, auth_username);
  }

  if (strcasecmp(ens_owner_address, oasis_wallet_address) == 0) {
    if (contract_debug_mode) {
      LM_INFO("Web3Auth: ENS validation successful! Addresses match: %s\n",
              ens_owner_address);
      LM_INFO("Web3Auth: ENS '%s' owner matches Oasis user '%s' wallet\n",
              username, auth_username);
    }
    return AUTHENTICATED; // 200 - Success
  } else {
    // Check if both addresses are non-zero
    if (strcmp(oasis_wallet_address,
               "0x0000000000000000000000000000000000000000") != 0) {
      LM_ERR("Web3Auth: Address mismatch - ENS owner: %s, Oasis wallet: %s\n",
             ens_owner_address, oasis_wallet_address);
      return NOT_AUTHENTICATED; // 401 - Invalid
    } else {
      LM_ERR("Web3Auth: No wallet address found in Oasis for %s\n",
             auth_username);
      return NOT_AUTHENTICATED; // 401 - Invalid
    }
  }
}

/**
 * Core blockchain verification function
 * This is the main authentication logic that replaces password-based auth
 */
int auth_web3_check_response(dig_cred_t *cred, str *method) {
  CURL *curl;
  CURLcode res;
  struct Web3ResponseData web3_response = {0};
  struct curl_slist *headers = NULL;
  int result = NOT_AUTHENTICATED;
  char username_str[256];
  char *call_data = NULL;

  // Extract username from credentials
  if (cred->username.user.len >= sizeof(username_str)) {
    LM_ERR("Web3Auth: Username too long (%d chars)\n", cred->username.user.len);
    return NOT_AUTHENTICATED;
  }

  memcpy(username_str, cred->username.user.s, cred->username.user.len);
  username_str[cred->username.user.len] = '\0';

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Authenticating user=%s, realm=%.*s\n", username_str,
            cred->realm.len, cred->realm.s);
    LM_INFO("Web3Auth: User provided response=%.*s\n", cred->response.len,
            cred->response.s);
  }

  curl = curl_easy_init();
  if (!curl) {
    LM_ERR("Web3Auth: Failed to initialize curl\n");
    return NOT_AUTHENTICATED;
  }

  // Extract parameters from SIP message context
  char realm_str[256], method_str[16], uri_str[256], nonce_str[256],
      response_str[256];

  // Extract realm
  if (cred->realm.len >= sizeof(realm_str)) {
    LM_ERR("Web3Auth: Realm too long (%d chars)\n", cred->realm.len);
    goto cleanup;
  }
  memcpy(realm_str, cred->realm.s, cred->realm.len);
  realm_str[cred->realm.len] = '\0';

  // Extract method (from the method parameter)
  if (method && method->len < sizeof(method_str)) {
    memcpy(method_str, method->s, method->len);
    method_str[method->len] = '\0';
  } else {
    strcpy(method_str, "REGISTER"); // Default method
  }

  // Extract URI and nonce from digest credentials
  if (cred->uri.len >= sizeof(uri_str)) {
    LM_ERR("Web3Auth: URI too long (%d chars)\n", cred->uri.len);
    goto cleanup;
  }
  memcpy(uri_str, cred->uri.s, cred->uri.len);
  uri_str[cred->uri.len] = '\0';

  if (cred->nonce.len >= sizeof(nonce_str)) {
    LM_ERR("Web3Auth: Nonce too long (%d chars)\n", cred->nonce.len);
    goto cleanup;
  }
  memcpy(nonce_str, cred->nonce.s, cred->nonce.len);
  nonce_str[cred->nonce.len] = '\0';

  // Extract user's response
  if (cred->response.len >= sizeof(response_str)) {
    LM_ERR("Web3Auth: Response too long (%d chars)\n", cred->response.len);
    goto cleanup;
  }
  memcpy(response_str, cred->response.s, cred->response.len);
  response_str[cred->response.len] = '\0';

  // Determine algorithm: 0 for MD5, 1 for SHA256, 2 for SHA512
  uint8_t algo = 0; // Default to MD5
  // Note: In the module, we need to get auth_algorithm from the base auth
  // module For now, defaulting to MD5

  // Convert hex string response to bytes
  int response_byte_len = strlen(response_str) / 2;
  unsigned char response_bytes[64]; // Max 64 bytes for SHA-512
  int actual_byte_len =
      hex_to_bytes(response_str, response_bytes, sizeof(response_bytes));

  if (actual_byte_len != response_byte_len) {
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
  size_t padded_len7 = ((actual_byte_len + 31) / 32) * 32; // For response bytes

  // Calculate offsets (selector + 7 offset words = 0xE0 for first string)
  size_t offset1 = 0xE0;
  size_t offset2 = offset1 + 32 + padded_len1;
  size_t offset3 = offset2 + 32 + padded_len2;
  size_t offset4 = offset3 + 32 + padded_len3;
  size_t offset5 = offset4 + 32 + padded_len4;
  size_t offset7 = offset5 + 32 + padded_len5;

  // Calculate total length needed for ABI encoding
  int total_len = 10 + (7 * 64) + (32 + padded_len1) + (32 + padded_len2) +
                  (32 + padded_len3) + (32 + padded_len4) + (32 + padded_len5) +
                  (32 + padded_len7);

  call_data = (char *)pkg_malloc(total_len * 2 +
                                 1); // *2 for hex encoding + null terminator
  if (!call_data) {
    LM_ERR("Web3Auth: Failed to allocate memory for ABI data\n");
    goto cleanup;
  }

  int pos = 0;

  // Function selector for authenticateUser
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "dd02fd8e");

  // Offset words (32 bytes each, as 64 hex chars)
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset1);
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset2);
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset3);
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset4);
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset5);

  // uint8 algo parameter (padded to 32 bytes)
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064x", algo);

  // Offset for response bytes
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", offset7);

  // String 1: username - length + padded data
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", len1);
  for (size_t i = 0; i < len1; i++) {
    pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
                    (unsigned char)username_str[i]);
  }
  for (size_t i = len1 * 2; i < padded_len1 * 2; i++) {
    call_data[pos++] = '0';
  }

  // String 2: realm - length + padded data
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", len2);
  for (size_t i = 0; i < len2; i++) {
    pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
                    (unsigned char)realm_str[i]);
  }
  for (size_t i = len2 * 2; i < padded_len2 * 2; i++) {
    call_data[pos++] = '0';
  }

  // String 3: method - length + padded data
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", len3);
  for (size_t i = 0; i < len3; i++) {
    pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
                    (unsigned char)method_str[i]);
  }
  for (size_t i = len3 * 2; i < padded_len3 * 2; i++) {
    call_data[pos++] = '0';
  }

  // String 4: uri - length + padded data
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", len4);
  for (size_t i = 0; i < len4; i++) {
    pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
                    (unsigned char)uri_str[i]);
  }
  for (size_t i = len4 * 2; i < padded_len4 * 2; i++) {
    call_data[pos++] = '0';
  }

  // String 5: nonce - length + padded data
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064lx", len5);
  for (size_t i = 0; i < len5; i++) {
    pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
                    (unsigned char)nonce_str[i]);
  }
  for (size_t i = len5 * 2; i < padded_len5 * 2; i++) {
    call_data[pos++] = '0';
  }

  // Bytes 7: response - length + padded data
  pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%064x",
                  actual_byte_len);
  for (int i = 0; i < actual_byte_len; i++) {
    pos += snprintf(call_data + pos, total_len * 2 + 1 - pos, "%02x",
                    response_bytes[i]);
  }
  for (size_t i = actual_byte_len * 2; i < padded_len7 * 2; i++) {
    call_data[pos++] = '0';
  }

  call_data[pos] = '\0';

  char payload[32768]; // Increased buffer size for larger call data
  snprintf(payload, sizeof(payload),
           "{\"jsonrpc\":\"2.0\",\"method\":\"eth_call\",\"params\":[{\"to\":"
           "\"%s\",\"data\":\"0x%s\"},\"latest\"],\"id\":1}",
           authentication_contract_address, call_data);

  if (contract_debug_mode) {
    const char *algo_name = (algo == 0)   ? "MD5"
                            : (algo == 1) ? "SHA-256"
                                          : "SHA-512";
    LM_INFO("Web3Auth: Algorithm: %s (%d)\n", algo_name, algo);
    LM_INFO("Web3Auth: Calling authenticateUser with payload: %s\n", payload);
  }

  curl_easy_setopt(curl, CURLOPT_URL, authentication_rpc_url);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, web3_curl_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &web3_response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)rpc_timeout);

  headers = curl_slist_append(NULL, "Content-Type: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    LM_ERR("Web3Auth: curl_easy_perform() failed: %s\n",
           curl_easy_strerror(res));
    goto cleanup;
  }

  if (!web3_response.memory) {
    LM_ERR("Web3Auth: No response from blockchain\n");
    goto cleanup;
  }

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Blockchain response: %s\n", web3_response.memory);
  }

  // Parse JSON response to extract the boolean result
  char *result_start = strstr(web3_response.memory, "\"result\":\"");
  if (!result_start) {
    LM_ERR("Web3Auth: Invalid blockchain response format\n");
    goto cleanup;
  }

  result_start += 10; // Skip "result":"
  char *result_end = strchr(result_start, '"');
  if (!result_end) {
    LM_ERR("Web3Auth: Malformed blockchain response\n");
    goto cleanup;
  }

  // Extract the result (should be 0x followed by 64 hex chars)
  char *hex_start = result_start;
  if (strncmp(hex_start, "0x", 2) == 0) {
    hex_start += 2;
  }

  int hex_len = result_end - hex_start;
  if (hex_len < 64) {
    LM_ERR(
        "Web3Auth: Invalid result length from blockchain: %d (expected 64)\n",
        hex_len);
    goto cleanup;
  }

  // Check if the last character is '1' (true) or '0' (false)
  char last_char = hex_start[hex_len - 1];
  if (last_char == '1') {
    if (contract_debug_mode) {
      LM_INFO("Web3Auth: Authentication successful! Contract returned true\n");
    }
    result = AUTHENTICATED;
  } else if (last_char == '0') {
    if (contract_debug_mode) {
      LM_INFO("Web3Auth: Authentication failed! Contract returned false\n");
    }
    result = NOT_AUTHENTICATED;
  } else {
    LM_ERR("Web3Auth: Invalid boolean result from contract: %c\n", last_char);
    result = NOT_AUTHENTICATED;
  }

cleanup:
  if (headers)
    curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  if (web3_response.memory)
    free(web3_response.memory);
  if (call_data)
    pkg_free(call_data);

  return result;
}

/**
 * Main Web3 authentication function that integrates with the base auth module
 */
int web3_digest_authenticate(struct sip_msg *msg, str *realm,
                             hdr_types_t hftype, str *method) {
  struct hdr_field *h;
  auth_body_t *cred;
  auth_cfg_result_t ret;
  auth_result_t rauth;
  char from_username[256] = {0};

  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Starting digest authentication for realm=%.*s\n",
            realm->len, realm->s);
  }

  // Extract username from "From" header field
  if (msg->from && msg->from->parsed) {
    struct to_body *from_body = (struct to_body *)msg->from->parsed;
    struct sip_uri parsed_uri;

    // Parse the URI to extract user part
    if (parse_uri(from_body->uri.s, from_body->uri.len, &parsed_uri) < 0) {
      LM_ERR("Web3Auth: Failed to parse From URI\n");
      return AUTH_ERROR;
    }

    if (parsed_uri.user.len > 0 &&
        parsed_uri.user.len < sizeof(from_username)) {
      memcpy(from_username, parsed_uri.user.s, parsed_uri.user.len);
      from_username[parsed_uri.user.len] = '\0';

      if (contract_debug_mode) {
        LM_INFO("Web3Auth: Extracted from username: %s\n", from_username);
      }
    } else {
      LM_ERR("Web3Auth: Invalid or missing username in From header\n");
      return AUTH_ERROR;
    }
  } else {
    LM_ERR("Web3Auth: No From header found\n");
    return AUTH_ERROR;
  }

  // Use the base auth module for pre-authentication processing
  switch (auth_api.pre_auth(msg, realm, hftype, &h, NULL)) {
  case NONCE_REUSED:
    LM_DBG("Web3Auth: nonce reused\n");
    ret = AUTH_NONCE_REUSED;
    goto end;
  case STALE_NONCE:
    LM_DBG("Web3Auth: stale nonce\n");
    ret = AUTH_STALE_NONCE;
    goto end;
  case NO_CREDENTIALS:
    LM_DBG("Web3Auth: no credentials\n");
    ret = AUTH_NO_CREDENTIALS;
    goto end;
  case ERROR:
  case BAD_CREDENTIALS:
    LM_DBG("Web3Auth: error or bad credentials\n");
    ret = AUTH_ERROR;
    goto end;
  case CREATE_CHALLENGE:
    LM_ERR("Web3Auth: CREATE_CHALLENGE is not a valid state\n");
    ret = AUTH_ERROR;
    goto end;
  case DO_RESYNCHRONIZATION:
    LM_ERR("Web3Auth: DO_RESYNCHRONIZATION is not a valid state\n");
    ret = AUTH_ERROR;
    goto end;
  case NOT_AUTHENTICATED:
    LM_DBG("Web3Auth: not authenticated\n");
    ret = AUTH_ERROR;
    goto end;
  case DO_AUTHENTICATION:
    break;
  case AUTHENTICATED:
    ret = AUTH_OK;
    goto end;
  }

  cred = (auth_body_t *)h->parsed;

  // Use ENS validation which includes fallback to normal Web3 authentication
  rauth = web3_ens_validate(from_username, &(cred->digest), method);

  // Handle different return codes from ENS validation
  if (rauth == AUTHENTICATED) {
    ret = AUTH_OK;
    // Use base auth module for post-authentication processing
    switch (auth_api.post_auth(msg, h, NULL)) {
    case AUTHENTICATED:
      break;
    default:
      ret = AUTH_ERROR;
      break;
    }
  } else if (rauth == 402) {
    // ENS validation failed - return specific error
    ret = AUTH_ERROR; // or define a specific AUTH_ENS_INVALID if available
    LM_ERR("Web3Auth: ENS validation failed for %s\n", from_username);
  } else {
    if (rauth == NOT_AUTHENTICATED)
      ret = AUTH_INVALID_PASSWORD;
    else
      ret = AUTH_ERROR;
  }

end:
  if (contract_debug_mode) {
    LM_INFO("Web3Auth: Authentication result: %d\n", ret);
  }

  return ret;
}
