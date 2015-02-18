/*
 * Copyright (C) 2001-2003 FhG Fokus
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


#ifndef _SMS_FUNCS_H
#define  _SMS_FUNCS_H

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include <termios.h>
#include "../../modules/tm/tm_load.h"


#define MAX_MODEMS    5       /* max number of modems */
#define MAX_NETWORKS  5       /* max number of networks */

#define MAX_CHAR_BUF 128        /* max length of character buffer */
#define MAX_CONFIG_PARAM 1024   /* max length of a config parameter */
#define MAX_SMS_LENGTH   160
#define MAX_SMS_PARTS    4      /* maximum number of parts for a sms */
#define MAX_QUEUED_MESSAGES 100 /* maximum nr of messages waiting to send */

#define SMS_HDR_BF_ADDR      "From "
#define SMS_HDR_BF_ADDR_LEN  (sizeof(SMS_HDR_BF_ADDR)-1)
#define SMS_HDR_AF_ADDR      " (if you reply DO NOT remove it)\r\n\r\n"
#define SMS_HDR_AF_ADDR_LEN  (sizeof(SMS_HDR_AF_ADDR)-1)
#define SMS_FOOTER           "\r\n\r\n[SIP-ROUTER.ORG]"
#define SMS_FOOTER_LEN       (sizeof(SMS_FOOTER)-1)
#define SMS_EDGE_PART        "( / )"
#define SMS_EDGE_PART_LEN    (sizeof(SMS_EDGE_PART)-1)
#define SMS_TRUNCATED        "(truncated)"
#define SMS_TRUNCATED_LEN    (sizeof(SMS_TRUNCATED)-1)

#define TIME_LEN   8          /* xx-xx-xx */
#define DATE_LEN   TIME_LEN

#define NO_REPORT  0
#define SMS_REPORT 1
#define CDS_REPORT 2

#define SMS_BODY_SCAN_NO	0
#define SMS_BODY_SCAN		1
#define SMS_BODY_SCAN_MIX	2

struct network {
	char name[MAX_CHAR_BUF+1];
	int  max_sms_per_call;
	int  pipe_out;
};

struct modem {
	char name[MAX_CHAR_BUF+1];
	char device[MAX_CHAR_BUF+1];
	char pin[MAX_CHAR_BUF+1];
	char smsc[MAX_CHAR_BUF+1];
	int  net_list[MAX_NETWORKS];
	struct termios oldtio;
	int  mode;
	int  retry;
	int  looping_interval;
	int  fd;
	int  baudrate;
	int  scan;
	char to[MAX_CHAR_BUF+1];
};

struct sms_msg {
	str  text;
	str  to;
	str  from;
	int  ref;
};

struct incame_sms {
	char sender[31];
	char name[64];
	char date[DATE_LEN];
	char time[TIME_LEN];
	char ascii[500];
	char smsc[31];
	int  userdatalength;
	int  is_statusreport;
	int  sms_id;
};


extern struct modem modems[MAX_MODEMS];
extern struct network networks[MAX_NETWORKS];
extern int    net_pipes_in[MAX_NETWORKS];
extern int    nr_of_networks;
extern int    nr_of_modems;
extern int    max_sms_parts;
extern str    domain;
extern int    *queued_msgs;
extern int    use_contact;
extern int    sms_report_type;
extern struct tm_binds tmb;

void modem_process(struct modem*);
int  push_on_network(struct sip_msg*, int);


#endif

