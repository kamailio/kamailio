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
 */

#include <osp/osp.h>
#include <osp/osputils.h>
#include "../../dprint.h"
#include "provider.h"

extern unsigned int _osp_sp_number;
extern char* _osp_sp_uris[];
extern unsigned long _osp_sp_weights[];
extern unsigned char* _osp_private_key;
extern unsigned char* _osp_local_certificate;
extern unsigned char* _osp_ca_certificate;
extern int _osp_ssl_lifetime;
extern int _osp_persistence;
extern int _osp_retry_delay;
extern int _osp_retry_limit;
extern int _osp_timeout;
extern int _osp_crypto_hw;
extern OSPTPROVHANDLE _osp_provider;

/*
 * Create a new OSP provider object per process
 * return 0 success, others failure
 */
int ospSetupProvider(void) 
{
    OSPTPRIVATEKEY privatekey;
    OSPTCERT localcert;
    OSPTCERT cacert;
    OSPTCERT* cacerts[1];
    int result;

    cacerts[0] = &cacert;

    if ((result = OSPPInit(_osp_crypto_hw)) != 0) {
        LM_ERR("failed to initalize OSP (%d)\n", result);
    } else if (OSPPUtilLoadPEMPrivateKey(_osp_private_key, &privatekey) != 0) {
        LM_ERR("failed to load private key from '%s'\n", _osp_private_key);
    } else if (OSPPUtilLoadPEMCert(_osp_local_certificate, &localcert) != 0) {
        LM_ERR("failed to load local certificate from '%s'\n",_osp_local_certificate);
    } else if (OSPPUtilLoadPEMCert(_osp_ca_certificate, &cacert) != 0) {
        LM_ERR("failed to load CA certificate from '%s'\n", _osp_ca_certificate);
    } else {
        result = OSPPProviderNew(
            _osp_sp_number,
            (const char**)_osp_sp_uris,
            _osp_sp_weights,
            "http://localhost:1234",
            &privatekey,
            &localcert,
            1,
            (const OSPTCERT**)cacerts,
            1,
            _osp_ssl_lifetime,
            _osp_sp_number,
            _osp_persistence,
            _osp_retry_delay,
            _osp_retry_limit,
            _osp_timeout,
            "",
            "",
            &_osp_provider);
        if (result != 0) {
            LM_ERR("failed to create provider (%d)\n", result);
        } else {
            LM_DBG("created new (per process) provider '%d'\n", _osp_provider);
            result = 0;
        }
    }

    /* 
     * Free space allocated while loading crypto information from PEM-encoded files.
     * There are some problems to free the memory, do not free them
     */
    if (privatekey.PrivateKeyData != NULL) {
        //free(privatekey.PrivateKeyData);
    }

    if (localcert.CertData != NULL) {
        //free(localcert.CertData);
    }
    
    if (cacert.CertData != NULL) {
        //free(localcert.CertData);
    }

    return result;
}

/*
 * Erase OSP provider object
 * return 0 success, others failure
 */
int ospDeleteProvider(void) 
{
    int result;

    if ((result = OSPPProviderDelete(_osp_provider, 0)) != 0) {
        LM_ERR("failed to erase provider '%d' (%d)\n", _osp_provider, result);
    }
    
    return result;
}

