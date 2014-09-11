/*
 * Kamailio osp module. 
 *
 * This module enables Kamailio to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
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
 * History:
 * ---------
 *  2006-03-13  RR functions are loaded via API function (bogdan)
 */

#include <osp/osp.h>
#include "../rr/api.h"
#include "../../modules/siputils/siputils.h"
#include "osp_mod.h"
#include "orig_transaction.h"
#include "term_transaction.h"
#include "usage.h"
#include "tm.h"
#include "provider.h"

MODULE_VERSION

extern unsigned int _osp_sp_number;
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
extern int _osp_redir_uri;
extern char _osp_PRIVATE_KEY[];
extern char _osp_LOCAL_CERTIFICATE[];
extern char _osp_CA_CERTIFICATE[];
extern char* _osp_snid_avp;
extern int_str _osp_snid_avpname;
extern unsigned short _osp_snid_avptype;
extern OSPTPROVHANDLE _osp_provider;

struct rr_binds osp_rr;
siputils_api_t osp_siputils;
int osp_index[OSP_DEF_SPS];

static int ospInitMod(void);
static void ospDestMod(void);
static int ospInitChild(int);
static int  ospVerifyParameters(void);
static void ospDumpParameters(void);

static cmd_export_t cmds[]={
    {"checkospheader",          (cmd_function)ospCheckHeader,      0, 0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"validateospheader",       (cmd_function)ospValidateHeader,   0, 0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"requestosprouting",       (cmd_function)ospRequestRouting,   0, 0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"checkosproute",           (cmd_function)ospCheckRoute,       0, 0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"prepareosproute",         (cmd_function)ospPrepareRoute,     0, 0, 0, BRANCH_ROUTE}, 
    {"prepareallosproutes",     (cmd_function)ospPrepareAllRoutes, 0, 0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"checkcallingtranslation", (cmd_function)ospCheckTranslation, 0, 0, 0, BRANCH_ROUTE}, 
    {"reportospusage",          (cmd_function)ospReportUsage,      1, 0, 0, REQUEST_ROUTE}, 
    {0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
    {"sp1_uri",                        PARAM_STRING, &_osp_sp_uris[0]},
    {"sp2_uri",                        PARAM_STRING, &_osp_sp_uris[1]},
    {"sp3_uri",                        PARAM_STRING, &_osp_sp_uris[2]},
    {"sp4_uri",                        PARAM_STRING, &_osp_sp_uris[3]},
    {"sp5_uri",                        PARAM_STRING, &_osp_sp_uris[4]},
    {"sp6_uri",                        PARAM_STRING, &_osp_sp_uris[5]},
    {"sp7_uri",                        PARAM_STRING, &_osp_sp_uris[6]},
    {"sp8_uri",                        PARAM_STRING, &_osp_sp_uris[7]},
    {"sp9_uri",                        PARAM_STRING, &_osp_sp_uris[8]},
    {"sp10_uri",                       PARAM_STRING, &_osp_sp_uris[9]},
    {"sp11_uri",                       PARAM_STRING, &_osp_sp_uris[10]},
    {"sp12_uri",                       PARAM_STRING, &_osp_sp_uris[11]},
    {"sp13_uri",                       PARAM_STRING, &_osp_sp_uris[12]},
    {"sp14_uri",                       PARAM_STRING, &_osp_sp_uris[13]},
    {"sp15_uri",                       PARAM_STRING, &_osp_sp_uris[14]},
    {"sp16_uri",                       PARAM_STRING, &_osp_sp_uris[15]},
    {"sp1_weight",                     INT_PARAM, &(_osp_sp_weights[0])},
    {"sp2_weight",                     INT_PARAM, &(_osp_sp_weights[1])},
    {"sp3_weight",                     INT_PARAM, &(_osp_sp_weights[2])},
    {"sp4_weight",                     INT_PARAM, &(_osp_sp_weights[3])},
    {"sp5_weight",                     INT_PARAM, &(_osp_sp_weights[4])},
    {"sp6_weight",                     INT_PARAM, &(_osp_sp_weights[5])},
    {"sp7_weight",                     INT_PARAM, &(_osp_sp_weights[6])},
    {"sp8_weight",                     INT_PARAM, &(_osp_sp_weights[7])},
    {"sp9_weight",                     INT_PARAM, &(_osp_sp_weights[8])},
    {"sp10_weight",                    INT_PARAM, &(_osp_sp_weights[9])},
    {"sp11_weight",                    INT_PARAM, &(_osp_sp_weights[10])},
    {"sp12_weight",                    INT_PARAM, &(_osp_sp_weights[11])},
    {"sp13_weight",                    INT_PARAM, &(_osp_sp_weights[12])},
    {"sp14_weight",                    INT_PARAM, &(_osp_sp_weights[13])},
    {"sp15_weight",                    INT_PARAM, &(_osp_sp_weights[14])},
    {"sp16_weight",                    INT_PARAM, &(_osp_sp_weights[15])},
    {"device_ip",                      PARAM_STRING, &_osp_device_ip},
    {"device_port",                    PARAM_STRING, &_osp_device_port},
    {"private_key",                    PARAM_STRING, &_osp_private_key},
    {"local_certificate",              PARAM_STRING, &_osp_local_certificate},
    {"ca_certificates",                PARAM_STRING, &_osp_ca_certificate},
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
    {"redirection_uri_format",         INT_PARAM, &_osp_redir_uri},
    {"source_networkid_avp",           PARAM_STRING, &_osp_snid_avp},
    {0,0,0} 
};

struct module_exports exports = {
    "osp",
    DEFAULT_DLFLAGS,    /* dlopen flags */
    cmds,
    params,
    0,                  /* exported statistics */
    0,                  /* exported MI functions */
    0,                  /* exported pseudo-variables */
    0,                  /* extra processes */
    ospInitMod,         /* module initialization function */
    0,                  /* response function*/
    ospDestMod,         /* destroy function */
    ospInitChild,       /* per-child init function */
};

/*
 * Initialize OSP module
 * return 0 success, -1 failure
 */
static int ospInitMod(void)
{
    bind_siputils_t bind_su;

    if (ospVerifyParameters() != 0) {
        /* At least one parameter incorrect -> error */
        return -1;   
    }

    /* Load the RR API */
    if (load_rr_api(&osp_rr) != 0) {
        LM_WARN("failed to load the RR API. Check if you load the rr module\n");
        LM_WARN("add_rr_param is required for reporting duration for OSP transactions\n");
        memset(&osp_rr, 0, sizeof(osp_rr));
    }

    /* Load the AUTH API */
    bind_su = (bind_siputils_t)find_export("bind_siputils", 1, 0);
    if ((bind_su == NULL) || (bind_su(&osp_siputils) != 0)) {
        LM_WARN("failed to load the SIPUTILS API. Check if you load the auth module.\n");
        LM_WARN("rpid_avp & rpid_avp_type is required for calling number translation\n");
        memset(&osp_siputils, 0, sizeof(osp_siputils));
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

    code = ospSetupProvider();

    LM_DBG("provider '%d' (%d)\n", _osp_provider, code);

    return 0;
}

/*
 * Verify parameters for OSP module
 * return 0 success, -1 failure
 */
static int ospVerifyParameters(void)
{
    int i;
    pv_spec_t avp_spec;
    str avp_str;
    int result = 0;

    /* Default location for the cert files is in the compile time variable CFG_DIR */
    if (_osp_private_key == NULL) {
        sprintf(_osp_PRIVATE_KEY, "%spkey.pem", CFG_DIR);
        _osp_private_key = (unsigned char*)_osp_PRIVATE_KEY;
    } 

    if (_osp_local_certificate == NULL) {
        sprintf(_osp_LOCAL_CERTIFICATE, "%slocalcert.pem", CFG_DIR);
        _osp_local_certificate = (unsigned char*)_osp_LOCAL_CERTIFICATE;
    }

    if (_osp_ca_certificate == NULL) {
        sprintf(_osp_CA_CERTIFICATE, "%scacert_0.pem", CFG_DIR);
        _osp_ca_certificate = (unsigned char*)_osp_CA_CERTIFICATE;
    }

    if (_osp_device_ip == NULL) {
        _osp_device_ip = "";
    }

    if (_osp_device_port == NULL) {
        _osp_device_port = "";
    }

    if (_osp_max_dests > OSP_DEF_DESTS || _osp_max_dests < 1) {
        _osp_max_dests = OSP_DEF_DESTS;    
        LM_WARN("max_destinations is out of range, reset to %d\n", OSP_DEF_DESTS); 
    }

    if (_osp_token_format < 0 || _osp_token_format > 2) {
        _osp_token_format = OSP_DEF_TOKEN;
        LM_WARN("token_format is out of range, reset to %d\n", OSP_DEF_TOKEN);
    }

    _osp_sp_number = 0;
    for (i = 0; i < OSP_DEF_SPS; i++) {
        if (_osp_sp_uris[i] != NULL) {
            if (_osp_sp_number != i) {
                _osp_sp_uris[_osp_sp_number] = _osp_sp_uris[i];
                _osp_sp_weights[_osp_sp_number] = _osp_sp_weights[i];
                _osp_sp_uris[i] = NULL;
                _osp_sp_weights[i] = OSP_DEF_WEIGHT;
            }
            osp_index[_osp_sp_number] = i + 1;
            _osp_sp_number++;
        }
    }

    if (_osp_sp_number == 0) {
        LM_ERR("at least one service point uri must be configured\n");
        result = -1;
    }

    if (_osp_snid_avp && *_osp_snid_avp) {
        avp_str.s = _osp_snid_avp;
        avp_str.len = strlen(_osp_snid_avp);
        if (pv_parse_spec(&avp_str, &avp_spec) == NULL ||
            avp_spec.type != PVT_AVP ||
            pv_get_avp_name(0, &(avp_spec.pvp), &_osp_snid_avpname, &_osp_snid_avptype) != 0)
        {
            LM_WARN("'%s' invalid AVP definition\n", _osp_snid_avp);
            _osp_snid_avpname.n = 0;
            _osp_snid_avptype = 0;
        }
    } else {
        _osp_snid_avpname.n = 0;
        _osp_snid_avptype = 0;
    }

    ospDumpParameters();

    return result;
}

/*
 * Dump OSP module configuration
 */
static void ospDumpParameters(void) 
{
    int i;

    LM_INFO("module configuration: ");
    LM_INFO("    number of service points '%d'", _osp_sp_number);
    for (i = 0; i < _osp_sp_number; i++) {
        LM_INFO("    sp%d_uri '%s' sp%d_weight '%ld' ", 
            osp_index[i], _osp_sp_uris[i], osp_index[i], _osp_sp_weights[i]);
    }
    LM_INFO("    device_ip '%s' device_port '%s' ", _osp_device_ip, _osp_device_port);
    LM_INFO("    private_key '%s' ", _osp_private_key);
    LM_INFO("    local_certificate '%s' ", _osp_local_certificate);
    LM_INFO("    ca_certificates '%s' ", _osp_ca_certificate);
    LM_INFO("    enable_crypto_hardware_support '%d' ", _osp_crypto_hw);
    LM_INFO("    token_format '%d' ", _osp_token_format);
    LM_INFO("    ssl_lifetime '%d' ", _osp_ssl_lifetime);
    LM_INFO("    persistence '%d' ", _osp_persistence);
    LM_INFO("    retry_delay '%d' ", _osp_retry_delay);
    LM_INFO("    retry_limit '%d' ", _osp_retry_limit);
    LM_INFO("    timeout '%d' ", _osp_timeout);
    LM_INFO("    validate_call_id '%d' ", _osp_validate_callid);
    LM_INFO("    use_rpid_for_calling_number '%d' ", _osp_use_rpid);
    LM_INFO("    redirection_uri_format '%d' ", _osp_redir_uri);
    LM_INFO("    max_destinations '%d'\n", _osp_max_dests);
    if (_osp_snid_avpname.n == 0) {
        LM_INFO("    source network ID disabled\n");
    } else if (_osp_snid_avptype & AVP_NAME_STR) {
        LM_INFO("    source network ID AVP name '%.*s'\n", _osp_snid_avpname.s.len, _osp_snid_avpname.s.s);
    } else {
        LM_INFO("    source network ID AVP ID '%d'\n", _osp_snid_avpname.n);
    }
}

