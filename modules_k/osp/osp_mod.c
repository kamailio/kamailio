/*
 * openser osp module. 
 *
 * This module enables openser to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ---------
 *  2006-03-13  RR functions are loaded via API function (bogdan)
 */

#include <osp/osp.h>
#include "../rr/api.h"
#include "../auth/api.h"
#include "osp_mod.h"
#include "orig_transaction.h"
#include "term_transaction.h"
#include "usage.h"
#include "tm.h"
#include "provider.h"

MODULE_VERSION

extern char* _osp_sp_uris[];
extern unsigned long _osp_sp_weights[];
extern char* _osp_device_ip;
extern char* _osp_device_port;
extern unsigned char* _osp_private_key;
extern unsigned char* _osp_local_certificate;
extern unsigned char* _osp_ca_certificate;
extern int _osp_crypto_hw;
extern int _osp_validate_callid;
extern int _osp_token_format;
extern int _osp_ssl_lifetime;
extern int _osp_persistence;
extern int _osp_retry_delay;
extern int _osp_retry_limit;
extern int _osp_timeout;
extern int _osp_max_dests;
extern int _osp_use_rpid;
extern char _osp_PRIVATE_KEY[];
extern char _osp_LOCAL_CERTIFICATE[];
extern char _osp_CA_CERTIFICATE[];
extern OSPTPROVHANDLE _osp_provider;

struct rr_binds osp_rr;
auth_api_t osp_auth;

static int ospInitMod(void);
static void ospDestMod(void);
static int ospInitChild(int);
static int  ospVerifyParameters(void);
static void ospDumpParameters(void);

static cmd_export_t cmds[]={
    {"checkospheader",          ospCheckHeader,      0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"validateospheader",       ospValidateHeader,   0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"requestosprouting",       ospRequestRouting,   0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"checkosproute",           ospCheckRoute,       0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"prepareosproute",         ospPrepareRoute,     0, 0, BRANCH_ROUTE}, 
    {"prepareallosproutes",     ospPrepareAllRoutes, 0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"checkcallingtranslation", ospCheckTranslation, 0, 0, BRANCH_ROUTE}, 
    {"reportospusage",          ospReportUsage,      1, 0, REQUEST_ROUTE}, 
    {0, 0, 0, 0, 0}
};

static param_export_t params[]={
    {"sp1_uri",                        STR_PARAM, &_osp_sp_uris[0]},
    {"sp1_weight",                     INT_PARAM, &(_osp_sp_weights[0])},
    {"sp2_uri",                        STR_PARAM, &_osp_sp_uris[1]},
    {"sp2_weight",                     INT_PARAM, &(_osp_sp_weights[1])},
    {"device_ip",                      STR_PARAM, &_osp_device_ip},
    {"device_port",                    STR_PARAM, &_osp_device_port},
    {"private_key",                    STR_PARAM, &_osp_private_key},
    {"local_certificate",              STR_PARAM, &_osp_local_certificate},
    {"ca_certificates",                STR_PARAM, &_osp_ca_certificate},
    {"enable_crypto_hardware_support", INT_PARAM, &_osp_crypto_hw},
    {"validate_callid",                INT_PARAM, &(_osp_validate_callid)},
    {"token_format",                   INT_PARAM, &_osp_token_format},
    {"ssl_lifetime",                   INT_PARAM, &_osp_ssl_lifetime},
    {"persistence",                    INT_PARAM, &_osp_persistence},
    {"retry_delay",                    INT_PARAM, &_osp_retry_delay},
    {"retry_limit",                    INT_PARAM, &_osp_retry_limit},
    {"timeout",                        INT_PARAM, &_osp_timeout},
    {"max_destinations",               INT_PARAM, &_osp_max_dests},
    {"use_rpid_for_calling_number",    INT_PARAM, &_osp_use_rpid},
    {0,0,0} 
};

struct module_exports exports = {
    "osp",
	DEFAULT_DLFLAGS, /* dlopen flags */
    cmds,
    params,
    0,            /* exported statistics */
    0,            /* exported MI functions */
	0,            /* exported pseudo-variables */
    ospInitMod,   /* module initialization function */
    0,            /* response function*/
    ospDestMod,   /* destroy function */
    ospInitChild, /* per-child init function */
};

/*
 * Initialize OSP module
 * return 0 success, -1 failure
 */
static int ospInitMod(void)
{
    bind_auth_t bind_auth;

    LOG(L_DBG, "osp: ospInitMod\n");

    if (ospVerifyParameters() != 0) {
        /* At least one parameter incorrect -> error */
        return -1;   
    }

    /* Load the RR API */
    if (load_rr_api(&osp_rr) != 0) {
        LOG(L_WARN, "osp: WARN: failed to load RR API\n");
        LOG(L_WARN, "osp: WARN: add_rr_param is required for reporting duration for OSP transactions\n");
        memset(&osp_rr, 0, sizeof(osp_rr));
    }

    /* Load the AUTH API */
    bind_auth = (bind_auth_t)find_export("bind_auth", 0, 0);
    if ((bind_auth == NULL) || (bind_auth(&osp_auth) != 0)) {
        LOG(L_WARN, "osp: WARN: failed to load AUTH API\n");
        LOG(L_WARN, "osp: WARN: rpid_avp & rpid_avp_type is required for calling number translation\n");
        memset(&osp_auth, 0, sizeof(osp_auth));
    }

    if (ospInitTm() < 0) {
        return -1;
    }

    /* everything is fine, initialization done */
    return 0;
}

/*
 * Destrroy OSP module
 */
static void ospDestMod(void)
{
    LOG(L_DBG, "osp: ospDestMod\n");
}

/*
 * Initializeild process of OSP module
 * param rank
 * return 0 success, -1 failure
 */
static int ospInitChild(
    int rank)
{
    int code = -1;

    LOG(L_DBG, "osp: ospInitChild\n");

    code = ospSetupProvider();

    LOG(L_DBG, "osp: provider '%i' (%d)\n", _osp_provider, code);

    return 0;
}

/*
 * Verify parameters for OSP module
 * return 0 success, -1 failure
 */
static int ospVerifyParameters(void)
{
    int result = 0;

    LOG(L_DBG, "osp: ospVerifyParamters\n");

    /* Default location for the cert files is in the compile time variable CFG_DIR */
    if (_osp_private_key == NULL) {
        sprintf(_osp_PRIVATE_KEY, "%spkey.pem", CFG_DIR);
        _osp_private_key = _osp_PRIVATE_KEY;
    } 

    if (_osp_local_certificate == NULL) {
        sprintf(_osp_LOCAL_CERTIFICATE, "%slocalcert.pem", CFG_DIR);
        _osp_local_certificate = _osp_LOCAL_CERTIFICATE;
    }

    if (_osp_ca_certificate == NULL) {
        sprintf(_osp_CA_CERTIFICATE, "%scacert_0.pem", CFG_DIR);
        _osp_ca_certificate = _osp_CA_CERTIFICATE;
    }

    if (_osp_device_ip == NULL) {
        _osp_device_ip = "";
    }

    if (_osp_device_port == NULL) {
        _osp_device_port = "";
    }

    if (_osp_max_dests > OSP_DEF_DESTS || _osp_max_dests < 1) {
        _osp_max_dests = OSP_DEF_DESTS;    
        LOG(L_WARN,
            "osp: WARN: max_destinations is out of range, reset to %d\n", 
            OSP_DEF_DESTS);
    }

    if (_osp_token_format < 0 || _osp_token_format > 2) {
        _osp_token_format = OSP_DEF_TOKEN;
        LOG(L_WARN, 
            "osp: WARN: token_format is out of range, reset to %d\n", 
            OSP_DEF_TOKEN);
    }

    if (_osp_sp_uris[1] == NULL) {
        _osp_sp_uris[1] = _osp_sp_uris[0];
    }

    if (_osp_sp_uris[0] == NULL) {
        LOG(L_ERR, "osp: ERROR: sp1_uri must be configured\n");
        result = -1;
    }

    ospDumpParameters();

    return result;
}

/*
 * Dump OSP module configuration
 */
static void ospDumpParameters(void) 
{
    LOG(L_INFO, 
        "osp: module configuration: "
        "sp1_uri '%s' "
        "sp1_weight '%ld' "
        "sp2_uri '%s' "
        "sp2_weight '%ld' "
        "device_ip '%s' "
        "device_port '%s' "
        "private_key '%s' "
        "local_certificate '%s' "
        "ca_certificates '%s' "
        "enable_crypto_hardware_support '%d' "
        "token_format '%d' "
        "ssl_lifetime '%d' "
        "persistence '%d' "
        "retry_delay '%d' "
        "retry_limit '%d' "
        "timeout '%d' "
        "validate_call_id '%d' "
        "use_rpid_for_calling_number '%d' "
        "max_destinations '%d'\n",
        _osp_sp_uris[0],
        _osp_sp_weights[0],
        _osp_sp_uris[1],
        _osp_sp_weights[1],
        _osp_device_ip,
        _osp_device_port,
        _osp_private_key,
        _osp_local_certificate,
        _osp_ca_certificate,
        _osp_crypto_hw,
        _osp_token_format,
        _osp_ssl_lifetime,
        _osp_persistence,
        _osp_retry_delay,
        _osp_retry_limit,
        _osp_timeout,
        _osp_validate_callid,
        _osp_use_rpid,
        _osp_max_dests);
}

