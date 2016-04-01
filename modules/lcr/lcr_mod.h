/*
 * $Id$
 *
 * Various lcr related constant, types, and external variables
 *
 * Copyright (C) 2005-2014 Juha Heinanen
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 * 2005-02-06: created by jh
 */

/*!
 * \file
 * \brief SIP-router lcr :: Various LCR related constant, types, and external variables
 * \ingroup lcr
 * Module: \ref lcr
 */


#ifndef LCR_MOD_H
#define LCR_MOD_H

#include <stdio.h>
#include <pcre.h>
#include "../../lib/kmi/mi.h"
#include "../../locking.h"
#include "../../parser/parse_uri.h"
#include "../../ip_addr.h"

#define MAX_PREFIX_LEN 16
#define MAX_URI_LEN 256
#define MAX_HOST_LEN 64
#define MAX_NO_OF_GWS 128
#define MAX_NAME_LEN 128
#define MAX_TAG_LEN 64
#define MAX_USER_LEN 64
#define MAX_PARAMS_LEN 64
#define MAX_NO_OF_REPLY_CODES 15

typedef enum sip_protos uri_transport;

struct rule_info {
    unsigned int rule_id;
    char prefix[MAX_PREFIX_LEN];
    unsigned short prefix_len;
    char from_uri[MAX_URI_LEN + 1];
    unsigned short from_uri_len;
    pcre *from_uri_re;
    char request_uri[MAX_URI_LEN + 1];
    unsigned short request_uri_len;
    pcre *request_uri_re;
    unsigned short stopper;
    unsigned int enabled;
    struct target *targets;
    struct rule_info *next;
};

struct rule_id_info {
    unsigned int rule_id;
    struct rule_info *rule_addr;
    struct rule_id_info *next;
};

struct target {
    unsigned short gw_index;
    unsigned short priority;
    unsigned short weight;
    struct target *next;
};

struct instance {
    unsigned short instance_id;
    struct instance *next;
};

/* gw states */
/* if state > GW_INACTIVE has not yet reached failure threshold */
#define GW_ACTIVE 0
#define GW_PINGING 1
#define GW_INACTIVE 2

struct gw_info {
    unsigned int gw_id;
    char gw_name[MAX_NAME_LEN];
    unsigned short gw_name_len;
    char scheme[5];
    unsigned short scheme_len;
    struct ip_addr ip_addr;
    char hostname[MAX_HOST_LEN];
    unsigned short hostname_len;
    unsigned int port;
    uri_transport transport_code;
    char transport[15];
    unsigned int transport_len;
    char params[MAX_PARAMS_LEN];
    unsigned short params_len;
    unsigned int strip;
    char prefix[MAX_PREFIX_LEN];
    unsigned short prefix_len;
    char tag[MAX_TAG_LEN];
    unsigned short tag_len;
    unsigned int flags;
    unsigned short state;
    char uri[MAX_URI_LEN];
    unsigned short uri_len;
    unsigned int defunct_until;
};

extern unsigned int lcr_rule_hash_size_param;

extern unsigned int lcr_count_param;

extern gen_lock_t *reload_lock;

extern struct gw_info **gw_pt;
extern struct rule_info ***rule_pt;
extern struct rule_id_info **rule_id_hash_table;

extern int reload_tables();
extern int rpc_defunct_gw(unsigned int, unsigned int, unsigned int);

#endif /* LCR_MOD_H */
