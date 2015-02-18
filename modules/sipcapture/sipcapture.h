/*
 * hep related structure
 *
 * Copyright (C) 2011-2014 Alexandr Dubovikov (QSC AG) (alexandr.dubovikov@gmail.com)
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

#ifndef _sipcapture_h
#define _sipcapture_h

struct _sipcapture_object {
	str method;
	str reply_reason;
	str ruri;
	str ruri_user;
	str from_user;
	str from_tag;
	str to_user;
	str to_tag;
	str pid_user;
	str contact_user;
	str auth_user;
	str callid;
	str callid_aleg;
	str via_1;
	str via_1_branch;
	str cseq;
	str diversion;
	str reason;
	str content_type;
	str authorization;
	str user_agent;
	str source_ip;
	int source_port;
	str destination_ip;
	int destination_port;
	str contact_ip;
	int contact_port;
	str originator_ip;
	int originator_port;
	int proto;
	int family;
	str rtp_stat;
	int type;
        long long tmstamp;
	str node;
	str msg;
#ifdef STATISTICS
	stat_var *stat;
#endif
};


struct hep_generic_recv;
int receive_logging_json_msg(char * buf, unsigned int len, struct hep_generic_recv *hg, char *log_table);

#endif /* _sipcapture_h */
