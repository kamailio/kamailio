/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *
 * History:
 * --------
 * 2003-04-04  grand acc cleanup (jiri)
 */

#ifndef _ACC_H
#define _ACC_H

/* what is printed if value unknonw */
#define NA "n/a"
#define NA_LEN (sizeof(NA)-1)
/* syslog prefix */
#define ACC "ACC: "
#define ACC_LEN (sizeof(ACC)-1)
/* leading text for a request accounted from a script */
#define ACC_REQUEST "request accounted: "
#define ACC_REQUEST_LEN (sizeof(ACC_REQUEST)-1)
#define ACC_MISSED "call missed: "
#define ACC_MISSED_LEN (sizeof(ACC_MISSED)-1)
#define ACC_ANSWERED "transaction answered: "
#define ACC_ANSWERED_LEN (sizeof(ACC_ANSWERED)-1)
#define ACC_ACKED "request acknowledged: "
#define ACC_ACKED_LEN (sizeof(ACC_ACKED)-1)

/* syslog attribute names */
#define A_CALLID "call_id"
#define A_CALLID_LEN (sizeof(A_CALLID)-1)
#define A_TOTAG "totag"
#define A_TOTAG_LEN (sizeof(A_TOTAG)-1)
#define A_FROM "from"
#define A_FROM_LEN (sizeof(A_FROM)-1)
#define A_FROMUSER "fromuser"
#define A_FROMUSER_LEN (sizeof(A_FROMUSER)-1)
#define A_IURI "i-uri"
#define A_IURI_LEN (sizeof(A_IURI)-1)
#define A_METHOD "method"
#define A_METHOD_LEN (sizeof(A_METHOD)-1)
#define A_OURI "o-uri"
#define A_OURI_LEN (sizeof(A_OURI)-1)
#define A_FROMTAG "fromtag"
#define A_FROMTAG_LEN (sizeof(A_FROMTAG)-1)
#define A_FROMURI "fromuri"
#define A_FROMURI_LEN (sizeof(A_FROMURI)-1)
#define A_STATUS "code"
#define A_STATUS_LEN (sizeof(A_STATUS)-1)
#define A_TO "to"
#define A_TO_LEN (sizeof(A_TO)-1)
#define A_TOURI "touri"
#define A_TOURI_LEN (sizeof(A_TOURI)-1)
#define A_TOUSER "touser"
#define A_TOUSER_LEN (sizeof(A_TOUSER)-1)
#define A_UID "uid"
#define A_UID_LEN (sizeof(A_UID)-1)
#define A_UP_IURI "userpart"
#define A_UP_IURI_LEN (sizeof(A_UP_IURI)-1)

#define A_SEPARATOR ", " /* must be shorter than ACC! */
#define A_SEPARATOR_LEN (sizeof(A_SEPARATOR)-1)
#define A_EQ "="
#define A_EQ_LEN (sizeof(A_EQ)-1)
#define A_EOL "\n\0"
#define A_EOL_LEN (sizeof(A_EOL)-1)




int acc_log_request( struct sip_msg *rq, struct hdr_field *to,
		str *txt, str* phrase);
void acc_log_missed( struct cell* t, struct sip_msg *reply,
	unsigned int code );
void acc_log_ack(  struct cell* t , struct sip_msg *ack );
void acc_log_reply(  struct cell* t , struct sip_msg *reply,
	unsigned int code);
#ifdef SQL_ACC
int acc_db_request( struct sip_msg *rq, struct hdr_field *to,
		str* phrase,  char *table, char *fmt);
void acc_db_missed( struct cell* t, struct sip_msg *reply,
	unsigned int code );
void acc_db_ack(  struct cell* t , struct sip_msg *ack );
void acc_db_reply(  struct cell* t , struct sip_msg *reply,
	unsigned int code);
#endif

inline static int skip_cancel(struct sip_msg *msg)
{
	return (msg->REQ_METHOD==METHOD_CANCEL) && report_cancels==0;
}


#endif
