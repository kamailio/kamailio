/*
 * Various lcr related constant, types, and external variables
 *
 * Copyright (C) 2005-2014 Juha Heinanen
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
 */

/*!
 * \file
 * \brief Kamailio lcr :: Various LCR related constant, types, and external variables
 * \ingroup lcr
 * Module: \ref lcr
 */


#ifndef LCR_MOD_H
#define LCR_MOD_H

#include <stdio.h>
#include <pcre.h>
#include "../../core/locking.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/ip_addr.h"

#define MAX_PREFIX_LEN 16
#define MAX_URI_LEN 256
#define MAX_HOST_LEN 64
#define MAX_NO_OF_GWS 128
#define MAX_NAME_LEN 128
#define MAX_TAG_LEN 64
#define MAX_USER_LEN 64
#define MAX_PARAMS_LEN 64
#define MAX_NO_OF_REPLY_CODES 15
#define MAX_MT_TVALUE_LEN 128

typedef enum sip_protos uri_transport;

struct rule_info
{
	unsigned int rule_id;
	char prefix[MAX_PREFIX_LEN];
	unsigned short prefix_len;
	char from_uri[MAX_URI_LEN + 1];
	unsigned short from_uri_len;
	char mt_tvalue[MAX_MT_TVALUE_LEN + 1];
	unsigned short mt_tvalue_len;
	pcre *from_uri_re;
	char request_uri[MAX_URI_LEN + 1];
	unsigned short request_uri_len;
	pcre *request_uri_re;
	unsigned short stopper;
	unsigned int enabled;
	struct target *targets;
	struct rule_info *next;
};

struct rule_id_info
{
	unsigned int rule_id;
	struct rule_info *rule_addr;
	struct rule_id_info *next;
};

struct matched_gw_info
{
	unsigned short gw_index;
	unsigned int rule_id;
	unsigned short prefix_len;
	unsigned short priority;
	unsigned int weight;
	unsigned short duplicate;
};

struct target
{
	unsigned short gw_index;
	unsigned short priority;
	unsigned short weight;
	struct target *next;
};

struct instance
{
	unsigned short instance_id;
	struct instance *next;
};

/* gw states */
/* if state > GW_INACTIVE has not yet reached failure threshold */
#define GW_ACTIVE 0
#define GW_PINGING 1
#define GW_INACTIVE 2

struct gw_info
{
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

	unsigned long rcv_gw_reqs;
	unsigned long rcv_gw_reqs_invite;
	unsigned long rcv_gw_reqs_cancel;
	unsigned long rcv_gw_reqs_ack;
	unsigned long rcv_gw_reqs_bye;
	unsigned long rcv_gw_reqs_info;
	unsigned long rcv_gw_reqs_register;
	unsigned long rcv_gw_reqs_subscribe;
	unsigned long rcv_gw_reqs_notify;
	unsigned long rcv_gw_reqs_message;
	unsigned long rcv_gw_reqs_options;
	unsigned long rcv_gw_reqs_prack;
	unsigned long rcv_gw_reqs_update;
	unsigned long rcv_gw_reqs_refer;
	unsigned long rcv_gw_reqs_publish;
	unsigned long rcv_gw_reqs_other;

	unsigned long rcv_gw_rpl;
	unsigned long rcv_gw_rpl_invite;
	unsigned long rcv_gw_rpl_invite_by_method[6];
	unsigned long rcv_gw_rpl_cancel;
	unsigned long rcv_gw_rpl_cancel_by_method[6];
	unsigned long rcv_gw_rpl_bye;
	unsigned long rcv_gw_rpl_bye_by_method[6];
	unsigned long rcv_gw_rpl_register;
	unsigned long rcv_gw_rpl_register_by_method[6];
	unsigned long rcv_gw_rpl_message;
	unsigned long rcv_gw_rpl_message_by_method[6];
	unsigned long rcv_gw_rpl_prack;
	unsigned long rcv_gw_rpl_prack_by_method[6];
	unsigned long rcv_gw_rpl_update;
	unsigned long rcv_gw_rpl_update_by_method[6];
	unsigned long rcv_gw_rpl_refer;
	unsigned long rcv_gw_rpl_refer_by_method[6];

	unsigned long rcv_gw_rpls_1xx;
	unsigned long rcv_gw_rpls_18x;
	unsigned long rcv_gw_rpls_2xx;
	unsigned long rcv_gw_rpls_3xx;
	unsigned long rcv_gw_rpls_4xx;
	unsigned long rcv_gw_rpls_401;
	unsigned long rcv_gw_rpls_404;
	unsigned long rcv_gw_rpls_407;
	unsigned long rcv_gw_rpls_480;
	unsigned long rcv_gw_rpls_486;
	unsigned long rcv_gw_rpls_5xx;
	unsigned long rcv_gw_rpls_6xx;
};

extern unsigned int lcr_rule_hash_size_param;

extern unsigned int lcr_count_param;

extern gen_lock_t *reload_lock;

extern struct gw_info **gw_pt;
extern struct rule_info ***rule_pt;
extern struct rule_id_info **rule_id_hash_table;

extern int load_gws_dummy(int lcr_id, str *ruri_user, str *from_uri,
		str *request_uri, unsigned int *gw_indexes);
extern int reload_tables();
extern int rpc_defunct_gw(unsigned int, unsigned int, unsigned int);
extern void reset_gw_stats(struct gw_info *gw);

/* lcr stats enable flag */
extern unsigned int lcr_stats_flag;

#endif /* LCR_MOD_H */
