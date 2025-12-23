/*
 * Arnacon Authentication Module Header
 *
 * Copyright (C) 2025 Jonathan Kandel
 */

#ifndef AUTH_ARNACON_H
#define AUTH_ARNACON_H

#include "../../core/parser/msg_parser.h"
#include "../../core/str.h"

/* Module parameters (extern declarations) */
extern char *ens_registry_address;
extern char *ens_name_wrapper_address;
extern char *rpc_url;
extern int signature_timeout;
extern int debug_mode;

#endif /* AUTH_ARNACON_H */
