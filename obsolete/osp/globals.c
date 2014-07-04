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

#include <stdio.h>
#include <osp/osp.h>
#include "osp_mod.h"

unsigned int _osp_sp_number;
char* _osp_sp_uris[OSP_DEF_SPS];
unsigned long _osp_sp_weights[OSP_DEF_SPS] = {
    OSP_DEF_WEIGHT, OSP_DEF_WEIGHT, OSP_DEF_WEIGHT, OSP_DEF_WEIGHT,
    OSP_DEF_WEIGHT, OSP_DEF_WEIGHT, OSP_DEF_WEIGHT, OSP_DEF_WEIGHT,
    OSP_DEF_WEIGHT, OSP_DEF_WEIGHT, OSP_DEF_WEIGHT, OSP_DEF_WEIGHT,
    OSP_DEF_WEIGHT, OSP_DEF_WEIGHT, OSP_DEF_WEIGHT, OSP_DEF_WEIGHT
};
char* _osp_device_ip = NULL;
char* _osp_device_port = NULL;
unsigned char* _osp_private_key = NULL;
unsigned char* _osp_local_certificate = NULL;
unsigned char* _osp_ca_certificate = NULL;
int _osp_crypto_hw = OSP_DEF_HW;
int _osp_validate_callid = OSP_DEF_CALLID;
int _osp_token_format = OSP_DEF_TOKEN;
int _osp_ssl_lifetime = OSP_DEF_SSLLIFE;
int _osp_persistence = OSP_DEF_PERSISTENCE;
int _osp_retry_delay = OSP_DEF_DELAY;
int _osp_retry_limit = OSP_DEF_RETRY;
int _osp_timeout = OSP_DEF_TIMEOUT;
int _osp_max_dests = OSP_DEF_DESTS;
int _osp_use_rpid = OSP_DEF_USERPID;
int _osp_redir_uri = OSP_DEF_REDIRURI;
char _osp_PRIVATE_KEY[OSP_KEYBUF_SIZE];
char _osp_LOCAL_CERTIFICATE[OSP_KEYBUF_SIZE];
char _osp_CA_CERTIFICATE[OSP_KEYBUF_SIZE];

OSPTPROVHANDLE _osp_provider = -1;

