/*
 * ser osp module. 
 *
 * This module enables ser to communicate with an Open Settlement 
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI 
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP 
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 * 
 * Copyright (C) 2001-2005 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <osp/osp.h>
#include "../../sr_module.h"
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
extern OSPTPROVHANDLE _osp_provider;

int osp_index[OSP_DEF_SPS];

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
    {"prepareospfirstroute",    ospPrepareFirstRoute,0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"prepareospnextroute",     ospPrepareNextRoute, 0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"prepareallosproutes",     ospPrepareAllRoutes, 0, 0, REQUEST_ROUTE|FAILURE_ROUTE}, 
    {"appendospheaders",        ospAppendHeaders,    0, 0, BRANCH_ROUTE}, 
    {"reportospusage",          ospReportUsage,      0, 0, REQUEST_ROUTE}, 
    {0, 0, 0, 0, 0}
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
    {0,0,0} 
};

struct module_exports exports = {
    "osp",
    cmds,
    0,            /* RPC methods */
    params,
    ospInitMod,   /* module initialization function */
    0,            /* response function*/
    ospDestMod,   /* destroy function */
    0,            /* oncancel function */
    ospInitChild, /* per-child init function */
};

/*
 * Initialize OSP module
 * return 0 success, -1 failure
 */
static int ospInitMod(void)
{
    LOG(L_DBG, "osp: ospInitMod\n");

    if (ospVerifyParameters() != 0) {
        /* At least one parameter incorrect -> error */
        return -1;   
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
    int i;
    int result = 0;

    LOG(L_DBG, "osp: ospVerifyParamters\n");

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
        LOG(L_ERR, "osp: ERROR: at least one service point uri must be configured\n");
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
    int i;

    LOG(L_INFO, "osp: module configuration: ");
    LOG(L_INFO, "    number of service points '%d'", _osp_sp_number);
    for (i = 0; i < _osp_sp_number; i++) {
        LOG(L_INFO,
            "    sp%d_uri '%s' sp%d_weight '%ld' ", 
            osp_index[i], _osp_sp_uris[i], osp_index[i], _osp_sp_weights[i]);
    }
    LOG(L_INFO, "    device_ip '%s' device_port '%s' ", _osp_device_ip, _osp_device_port);
    LOG(L_INFO, "    private_key '%s' ", _osp_private_key);
    LOG(L_INFO, "    local_certificate '%s' ", _osp_local_certificate);
    LOG(L_INFO, "    ca_certificates '%s' ", _osp_ca_certificate);
    LOG(L_INFO, "    enable_crypto_hardware_support '%d' ", _osp_crypto_hw);
    LOG(L_INFO, "    token_format '%d' ", _osp_token_format);
    LOG(L_INFO, "    ssl_lifetime '%d' ", _osp_ssl_lifetime);
    LOG(L_INFO, "    persistence '%d' ", _osp_persistence);
    LOG(L_INFO, "    retry_delay '%d' ", _osp_retry_delay);
    LOG(L_INFO, "    retry_limit '%d' ", _osp_retry_limit);
    LOG(L_INFO, "    timeout '%d' ", _osp_timeout);
    LOG(L_INFO, "    validate_call_id '%d' ", _osp_validate_callid);
    LOG(L_INFO, "    use_rpid_for_calling_number '%d' ", _osp_use_rpid);
    LOG(L_INFO, "    redirection_uri_format '%d' ", _osp_redir_uri);
    LOG(L_INFO, "    max_destinations '%d'\n", _osp_max_dests);
}

