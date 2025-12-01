/*
 * Arnacon Core Authentication Header
 * 
 * Copyright (C) 2025 Jonathan Kandel
 */

#ifndef ARNACON_CORE_H
#define ARNACON_CORE_H

#include <stdint.h>
#include <stddef.h>

// Configuration constants
#define MAX_ENS_LENGTH 256
#define MAX_ADDRESS_LENGTH 43  // 0x + 40 hex chars + null terminator
#define MAX_SIGNATURE_LENGTH 132  // 0x + 130 hex chars + null terminator
#define MAX_UUID_LENGTH 64
#define MAX_RESPONSE_SIZE 4096

// Return codes
#define ARNACON_SUCCESS 200
#define ARNACON_FAILURE 404
#define ARNACON_ERROR -1

// Structure for HTTP response data
struct ResponseData {
    char *memory;
    size_t size;
};

// Structure for parsed X-Data
struct XData {
    char uuid[MAX_UUID_LENGTH];
    uint64_t timestamp;
};

/**
 * Initialize the core authentication system
 * @return 0 on success, -1 on error
 */
int arnacon_core_init(void);

/**
 * Cleanup the core authentication system
 */
void arnacon_core_cleanup(void);

/**
 * Main authentication function
 * @param ens ENS name
 * @param x_data X-Data parameter
 * @param x_sign X-Sign parameter  
 * @param registry_address ENS Registry contract address
 * @param rpc_url RPC endpoint URL
 * @param timeout_seconds Signature timeout in seconds
 * @param debug Enable debug logging
 * @return 200 on success, 404 on failure, -1 on error
 */
int arnacon_core_authenticate(const char *ens, const char *x_data, const char *x_sign,
                             const char *registry_address, const char *rpc_url, 
                             int timeout_seconds, int debug);

/**
 * Check if user exists (ENS has an owner)
 * @param ens ENS name
 * @param registry_address ENS Registry contract address
 * @param rpc_url RPC endpoint URL
 * @param debug Enable debug logging
 * @return 1 if user exists, 0 if not, -1 on error
 */
int arnacon_core_user_exists(const char *ens, const char *registry_address,
                            const char *rpc_url, int debug);

/**
 * Compute ENS namehash for a given domain name
 * @param name The ENS domain name
 * @param hash_hex Output buffer for the hex-encoded hash (65 bytes minimum)
 * @return 0 on success, -1 on error
 */
int compute_namehash(const char *name, char *hash_hex);

/**
 * Make a blockchain RPC call to get ENS owner with dynamic Name Wrapper detection
 * @param ens_name The ENS domain name
 * @param owner_address Output buffer for the owner address (43 bytes minimum)
 * @param registry_address ENS Registry contract address
 * @param rpc_url RPC endpoint URL
 * @param debug Enable debug logging
 * @return 0 on success, -1 on error
 */
int get_ens_owner(const char *ens_name, char *owner_address, 
                 const char *registry_address, const char *rpc_url, int debug);

/**
 * Parse X-Data parameter to extract UUID and timestamp
 * @param x_data The X-Data string
 * @param parsed_data Output structure with parsed data
 * @return 0 on success, -1 on error
 */
int parse_x_data(const char *x_data, struct XData *parsed_data);

/**
 * Validate timestamp against current time
 * @param timestamp The timestamp to validate
 * @param max_age_seconds Maximum age in seconds
 * @return 0 if valid, -1 if expired
 */
int validate_timestamp(uint64_t timestamp, int max_age_seconds);

/**
 * Recover Ethereum address from ECDSA signature
 * @param message The signed message
 * @param signature The signature (hex string starting with 0x)
 * @param recovered_address Output buffer for recovered address (43 bytes minimum)
 * @param debug Enable debug logging
 * @return 0 on success, -1 on error
 */
int recover_ethereum_address(const char *message, const char *signature, 
                            char *recovered_address, int debug);

/**
 * Convert hex string to bytes
 * @param hex_str Input hex string
 * @param bytes Output byte array
 * @param max_bytes Maximum number of bytes to convert
 * @return Number of bytes converted, -1 on error
 */
int hex_to_bytes(const char *hex_str, unsigned char *bytes, size_t max_bytes);

/**
 * Convert bytes to hex string
 * @param bytes Input byte array
 * @param len Length of byte array
 * @param hex_str Output hex string buffer
 */
void bytes_to_hex(const unsigned char *bytes, size_t len, char *hex_str);

/**
 * Normalize Ethereum address (convert to lowercase and ensure 0x prefix)
 * @param address Input address
 * @param normalized Output normalized address (43 bytes minimum)
 * @return 0 on success, -1 on error
 */
int normalize_ethereum_address(const char *address, char *normalized);

/**
 * cURL callback function for collecting response data
 * @param contents Response data
 * @param size Size of each element
 * @param nmemb Number of elements
 * @param userp User data (ResponseData structure)
 * @return Number of bytes processed
 */
size_t response_write_callback(void *contents, size_t size, size_t nmemb, struct ResponseData *response);

#endif /* ARNACON_CORE_H */
