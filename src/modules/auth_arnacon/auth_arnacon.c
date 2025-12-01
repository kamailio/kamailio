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
#include <string.h>
#include <stdlib.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/pvar.h"
#include "../../modules/auth/api.h"

#include "auth_arnacon.h"
#include "arnacon_core.h"

MODULE_VERSION

/* Module parameters */
static char *arnacon_ens_registry_address = "0x16742E546bF92118F7dfdbEF5170E44C47ae254b";
static char *arnacon_rpc_url = "https://polygon-rpc.com";
static int arnacon_signature_timeout = 30;
static int arnacon_debug_mode = 0;

/* Forward declarations */
static int mod_init(void);
static int child_init(int rank);
static void destroy(void);
static int arnacon_authenticate_user(struct sip_msg *msg, char *ens_name_param, char *x_data_param, char *x_sign_param);
static int arnacon_user_exists(struct sip_msg *msg, char *ens_name_param, char *str2);

/* Module interface */
static cmd_export_t cmds[] = {
    {"arnacon_authenticate", (cmd_function)arnacon_authenticate_user, 3, 0, 0, REQUEST_ROUTE},
    {"arnacon_user_exists", (cmd_function)arnacon_user_exists, 1, 0, 0, REQUEST_ROUTE},
    {0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
    {"ens_registry_address", PARAM_STRING, &arnacon_ens_registry_address},
    {"rpc_url", PARAM_STRING, &arnacon_rpc_url},
    {"signature_timeout", PARAM_INT, &arnacon_signature_timeout},
    {"debug_mode", PARAM_INT, &arnacon_debug_mode},
    {0, 0, 0}
};

struct module_exports exports = {
    "auth_arnacon",    /* module name */
    DEFAULT_DLFLAGS,   /* dlopen flags */
    cmds,              /* exported functions */
    params,            /* exported parameters */
    0,                 /* exported RPC functions */
    0,                 /* exported pseudo-variables */
    0,                 /* response function */
    mod_init,          /* module initialization function */
    child_init,        /* per child init function */
    destroy            /* destroy function */
};

/**
 * Module initialization function
 */
static int mod_init(void)
{
    LM_INFO("Initializing...");
    
    if (arnacon_debug_mode) {
        LM_INFO("Debug mode enabled");
        LM_INFO("ENS Registry: %s", arnacon_ens_registry_address);
        LM_INFO("RPC URL: %s", arnacon_rpc_url);
        LM_INFO("Signature timeout: %d seconds", arnacon_signature_timeout);
        LM_INFO("Name Wrapper detection: Dynamic (no configuration needed)");
    }
    
    /* Initialize curl globally */
    if (arnacon_core_init() != 0) {
        LM_ERR("Failed to initialize core");
        return -1;
    }
    
    LM_INFO("Initialized successfully");
    return 0;
}

/**
 * Child initialization function
 */
static int child_init(int rank)
{
    LM_DBG("Child process %d initialized", rank);
    return 0;
}

/**
 * Module destroy function
 */
static void destroy(void)
{
    LM_INFO("Shutting down");
    arnacon_core_cleanup();
}

/**
 * Extract string parameter from Kamailio function call
 * Handles both literal strings and pseudo-variables
 */
static int get_str_param(struct sip_msg *msg, char *param, str *result)
{
    pv_spec_t spec;
    pv_value_t val;
    
    if (!param || !result) {
        return -1;
    }
    
    /* Check if it's a pseudo-variable (starts with $) */
    if (param[0] == '$') {
        /* Parse the pseudo-variable specification */
        str param_str;
        param_str.s = param;
        param_str.len = strlen(param);
        
        if (pv_parse_spec(&param_str, &spec) < 0) {
            LM_ERR("Failed to parse pseudo-variable: %s", param);
            return -1;
        }
        
        /* Get the value of the pseudo-variable */
        if (pv_get_spec_value(msg, &spec, &val) != 0) {
            LM_ERR("Failed to get value of pseudo-variable: %s", param);
            return -1;
        }
        
        /* Check if we got a valid string value */
        if (val.flags & PV_VAL_NULL) {
            LM_ERR("Pseudo-variable %s is null", param);
            return -1;
        }
        
        if (!(val.flags & PV_VAL_STR)) {
            LM_ERR("Pseudo-variable %s is not a string", param);
            return -1;
        }
        
        *result = val.rs;
        
        if (arnacon_debug_mode) {
            LM_DBG("Resolved pseudo-variable %s to: %.*s", param, result->len, result->s);
        }
        
        return 0;
    } else {
        /* Treat as literal string */
        result->s = param;
        result->len = strlen(param);
        return 0;
    }
}

/**
 * Extract X-Data and X-Sign headers from SIP message
 */
static int extract_auth_headers(struct sip_msg *msg, str *x_data, str *x_sign)
{
    struct hdr_field *hdr;
    
    /* Look for X-Data and X-Sign headers */
    for (hdr = msg->headers; hdr; hdr = hdr->next) {
        if (hdr->type == HDR_OTHER_T) {
            if (hdr->name.len == 6 && strncasecmp(hdr->name.s, "X-Data", 6) == 0) {
                *x_data = hdr->body;
                /* Trim whitespace */
                while (x_data->len > 0 && (x_data->s[0] == ' ' || x_data->s[0] == '\t')) {
                    x_data->s++;
                    x_data->len--;
                }
                while (x_data->len > 0 && (x_data->s[x_data->len-1] == ' ' || x_data->s[x_data->len-1] == '\t' || x_data->s[x_data->len-1] == '\r' || x_data->s[x_data->len-1] == '\n')) {
                    x_data->len--;
                }
            }
            else if (hdr->name.len == 6 && strncasecmp(hdr->name.s, "X-Sign", 6) == 0) {
                *x_sign = hdr->body;
                /* Trim whitespace */
                while (x_sign->len > 0 && (x_sign->s[0] == ' ' || x_sign->s[0] == '\t')) {
                    x_sign->s++;
                    x_sign->len--;
                }
                while (x_sign->len > 0 && (x_sign->s[x_sign->len-1] == ' ' || x_sign->s[x_sign->len-1] == '\t' || x_sign->s[x_sign->len-1] == '\r' || x_sign->s[x_sign->len-1] == '\n')) {
                    x_sign->len--;
                }
            }
        }
    }
    
    return (x_data->len > 0 && x_sign->len > 0) ? 0 : -1;
}

/**
 * Main authentication function
 * Usage: arnacon_authenticate("ens_name", "x_data", "x_sign")
 * Or: arnacon_authenticate("$fU", "", "") - to extract from headers
 */
static int arnacon_authenticate_user(struct sip_msg *msg, char *ens_name_param, char *x_data_param, char *x_sign_param)
{
    str ens_name, x_data, x_sign;
    char ens_name_buf[256], x_data_buf[256], x_sign_buf[256];
    int result;
    
    if (arnacon_debug_mode) {
        LM_DBG("Authenticate function called");
    }
    
    /* Extract ENS name parameter */
    if (get_str_param(msg, ens_name_param, &ens_name) != 0) {
        LM_ERR("Failed to get ENS name parameter");
        return -1;
    }
    
    /* Check if we should extract headers automatically */
    if (strlen(x_data_param) == 0 || strlen(x_sign_param) == 0) {
        /* Extract from headers */
        if (extract_auth_headers(msg, &x_data, &x_sign) != 0) {
            LM_ERR("Failed to extract X-Data and X-Sign headers");
            return -1;
        }
    } else {
        /* Use provided parameters */
        if (get_str_param(msg, x_data_param, &x_data) != 0 ||
            get_str_param(msg, x_sign_param, &x_sign) != 0) {
            LM_ERR("Failed to get X-Data or X-Sign parameters");
            return -1;
        }
    }
    
    /* Copy to null-terminated buffers */
    if (ens_name.len >= sizeof(ens_name_buf) ||
        x_data.len >= sizeof(x_data_buf) ||
        x_sign.len >= sizeof(x_sign_buf)) {
        LM_ERR("Parameter too long");
        return -1;
    }
    
    memcpy(ens_name_buf, ens_name.s, ens_name.len);
    ens_name_buf[ens_name.len] = '\0';
    
    memcpy(x_data_buf, x_data.s, x_data.len);
    x_data_buf[x_data.len] = '\0';
    
    memcpy(x_sign_buf, x_sign.s, x_sign.len);
    x_sign_buf[x_sign.len] = '\0';
    
    if (arnacon_debug_mode) {
        LM_DBG("Authentication request: ENS=%s", ens_name_buf);
        LM_DBG("X-Data: %s", x_data_buf);
        LM_DBG("X-Sign: %s", x_sign_buf);
    }
    
    /* Call core authentication function */
    result = arnacon_core_authenticate(
        ens_name_buf, 
        x_data_buf, 
        x_sign_buf,
        arnacon_ens_registry_address,
        arnacon_rpc_url,
        arnacon_signature_timeout,
        arnacon_debug_mode
    );
    
    if (arnacon_debug_mode) {
        LM_DBG("Authentication result: %d", result);
    }
    
    /* Return 1 for success, -1 for failure (Kamailio convention) */
    return (result == 200) ? 1 : -1;
}

/**
 * Check if user exists (ENS has an owner)
 * Usage: arnacon_user_exists("ens_name")
 */
static int arnacon_user_exists(struct sip_msg *msg, char *ens_name_param, char *str2)
{
    str ens_name;
    char ens_name_buf[256];
    int result;
    
    if (arnacon_debug_mode) {
        LM_DBG("User exists function called");
    }
    
    /* Extract ENS name parameter */
    if (get_str_param(msg, ens_name_param, &ens_name) != 0) {
        LM_ERR("Failed to get ENS name parameter");
        return -1;
    }
    
    /* Copy to null-terminated buffer */
    if (ens_name.len >= sizeof(ens_name_buf)) {
        LM_ERR("ENS name too long");
        return -1;
    }
    
    memcpy(ens_name_buf, ens_name.s, ens_name.len);
    ens_name_buf[ens_name.len] = '\0';
    
    if (arnacon_debug_mode) {
        LM_DBG("Checking if user exists: ENS=%s", ens_name_buf);
    }
    
    /* Call core user exists function */
    result = arnacon_core_user_exists(
        ens_name_buf,
        arnacon_ens_registry_address,
        arnacon_rpc_url,
        arnacon_debug_mode
    );
    
    if (arnacon_debug_mode) {
        LM_DBG("User exists result: %d", result);
    }
    
    /* Return 1 if user exists, -1 if not */
    return (result == 1) ? 1 : -1;
}
