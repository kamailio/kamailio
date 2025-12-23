/*
 * Arnacon Core Authentication Header
 *
 * Copyright (C) 2025 Jonathan Kandel
 */

#ifndef ARNACON_CORE_H
#define ARNACON_CORE_H

#include <stddef.h>
#include <stdint.h>

/* Configuration constants - prefixed with ARNACON_ for namespace */
#define ARNACON_MAX_ENS_LENGTH 256
#define ARNACON_MAX_ADDRESS_LENGTH 43 /* 0x + 40 hex chars + null terminator */
#define ARNACON_MAX_SIGNATURE_LENGTH \
	132 /* 0x + 130 hex chars + null terminator */
#define ARNACON_MAX_UUID_LENGTH 64
#define ARNACON_MAX_RESPONSE_SIZE 4096

/* Return codes */
#define ARNACON_SUCCESS 200
#define ARNACON_FAILURE 404
#define ARNACON_ERROR -1

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
 * @param ens ENS name or Ethereum address
 * @param x_data X-Data parameter (uuid:timestamp)
 * @param x_sign X-Sign parameter (signature)
 * @param registry_address ENS Registry contract address
 * @param rpc_url RPC endpoint URL
 * @param timeout_seconds Signature timeout in seconds
 * @param debug Enable debug logging
 * @return ARNACON_SUCCESS (200) on success, ARNACON_FAILURE (404) on failure,
 *         ARNACON_ERROR (-1) on error
 */
int arnacon_core_authenticate(const char *ens, const char *x_data,
		const char *x_sign, const char *registry_address, const char *rpc_url,
		int timeout_seconds, int debug);

/**
 * Check if user exists (ENS has an owner or address is valid)
 * @param ens ENS name or Ethereum address
 * @param registry_address ENS Registry contract address
 * @param rpc_url RPC endpoint URL
 * @param debug Enable debug logging
 * @return 1 if user exists, 0 if not, -1 on error
 */
int arnacon_core_user_exists(const char *ens, const char *registry_address,
		const char *rpc_url, int debug);

#endif /* ARNACON_CORE_H */
