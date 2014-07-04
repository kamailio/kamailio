/**
 * $Id$
 *
 * xcap_server module - builtin XCAP server
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailo is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
		       
#ifndef _XCAP_MISC_H_
#define _XCAP_MISC_H_

#include "../../str.h"
#include "../../sr_module.h"
#include "../../pvar.h"

#define XCAP_MAX_URI_SIZE	255
/* Node Selector Separator */
#define XCAP_NSS	"~~"

typedef struct xcap_uri {
	char buf[XCAP_MAX_URI_SIZE+1];
	str uri;
	str root;
	str auid;
	int type;
	str tree;
	str xuid;
	str file;
	str adoc;
	str rdoc;
	char *nss;
	str node;
	str target;
	str domain;
} xcap_uri_t;

typedef struct xcaps_auid_list {
	str auid;  /* auid value */
	char term; /* ending char (next one after auid) */
	int type;  /* internaly type id for auid */
} xcaps_auid_list_t;

extern xcaps_auid_list_t xcaps_auid_list[];

int xcap_parse_uri(str *huri, str *xroot, xcap_uri_t *xuri);
int xcaps_xpath_set(str *inbuf, str *xpaths, str *val, str *outbuf);
int xcaps_xpath_get(str *inbuf, str *xpaths, str *outbuf);
int xcaps_check_doc_validity(str *doc);

int pv_get_xcap_uri(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_set_xcap_uri(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);
int pv_parse_xcap_uri_name(pv_spec_p sp, str *in);

#endif
