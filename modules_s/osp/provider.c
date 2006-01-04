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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "osp_mod.h"
#include "provider.h"
#include "../../sr_module.h"
#include "osp/osputils.h"
#include "../../data_lump_rpl.h"
#include "../../mem/mem.h"

extern char* _spURIs[2];
extern int   _spWeights[2];

extern char* _private_key;
extern char* _local_certificate;
extern char* _ca_certificate;

extern int   _ssl_lifetime;
extern int   _persistence;
extern int   _retry_delay;
extern int   _retry_limit;
extern int   _timeout;
extern int   _crypto_hw_support;
extern OSPTPROVHANDLE _provider;


int setup_provider() {

	int result = 1;
	OSPTCERT localcert;
	OSPTCERT cacert;
	OSPTPRIVATEKEY privatekey;
	OSPTCERT *cacerts[1];

	cacerts[0] = &cacert;

	if ( (result = OSPPInit(_crypto_hw_support)) != 0 ) {
		ERR("osp: setup_provider: could not initalize libosp. (%i)\n", result);
	} else if ( OSPPUtilLoadPEMPrivateKey ((unsigned char*)_private_key, &privatekey) != 0 ) {
		ERR("osp: setup_provider: could not load private key from %s\n", _private_key);
	} else if ( OSPPUtilLoadPEMCert ((unsigned char*)_local_certificate, &localcert) != 0 ) {
		ERR("osp: setup_provider: could not load local certificate from %s\n",_local_certificate);
	} else	if ( OSPPUtilLoadPEMCert ((unsigned char*)_ca_certificate, &cacert) != 0 ) {
		ERR("osp: setup_provider: could not load CA certificate from %s\n", _ca_certificate);
	} else if ( 0 != (result = OSPPProviderNew(
				2,
				(const char **)_spURIs,
				(unsigned long *)_spWeights,
				"http://localhost:1234",
				&privatekey,
				&localcert,
				1,
				(const OSPTCERT **)cacerts,
				1,
				_ssl_lifetime,
				2,
				_persistence,
				_retry_delay,
				_retry_limit,
				_timeout,
				"",
				"",
				&_provider))) {
		ERR("osp: setup_provider: could not create provider. (%i)\n", result);
	} else {
		DBG("osp: Successfully created a new (per process) provider object, handle (%d)\n",_provider);
		result = 0;
	}

	/* Free space allocated while loading crypto information from PEM-encoded files */
	if (localcert.CertData != NULL) {
		//free(localcert.CertData);
	}
	
	if (cacert.CertData != NULL) {
		//free(localcert.CertData);
	}

	if (privatekey.PrivateKeyData != NULL) {
		//free(privatekey.PrivateKeyData);
	}

	return result;
}


int delete_provider() {
	int result;

	DBG("osp: Deleting provider object\n");

	if (0 != (result = OSPPProviderDelete(_provider,0))) {
		ERR("osp: problems deleting provider object, handle (%d), error (%d)\n",_provider,result);
	}
	
	return result;
}
