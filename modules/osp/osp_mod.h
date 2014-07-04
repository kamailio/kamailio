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

#ifndef _OSP_MOD_H_
#define _OSP_MOD_H_

#define MODULE_RETURNCODE_TRUE          1
#define MODULE_RETURNCODE_STOPROUTE     0
#define MODULE_RETURNCODE_FALSE         -1
#define MODULE_RETURNCODE_ERROR         -2

#define OSP_DEF_SPS                     16
#define OSP_DEF_WEIGHT                  1000
#define OSP_DEF_HW                      0
#define OSP_DEF_CALLID                  1    /* Validate call ids, set to 0 to disable */
#define OSP_DEF_TOKEN                   2
#define OSP_DEF_SSLLIFE                 300
#define OSP_DEF_PERSISTENCE             (60 * 1000)
#define OSP_DEF_DELAY                   0
#define OSP_DEF_RETRY                   2
#define OSP_DEF_TIMEOUT                 (60 * 1000)
#define OSP_DEF_DESTS                   5
#define OSP_DEF_USERPID                 1
#define OSP_DEF_REDIRURI                0   /* 0 for "xxxxxxxxxx@xxx.xxx.xxx.xxx", 1 for "<xxxxxxxxxx@xxx.xxx.xxx.xxx>" format */
#define OSP_DEF_SNIDAVP                 "$avp(s:_osp_source_networkid_)"

#define OSP_KEYBUF_SIZE                 256
#define OSP_STRBUF_SIZE                 256
#define OSP_E164BUF_SIZE                1024
#define OSP_TOKENBUF_SIZE               2048
#define OSP_HEADERBUF_SIZE              3072

#endif /* _OSP_MOD_H_ */

