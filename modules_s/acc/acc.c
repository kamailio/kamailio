/*
 *
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
 */


#include <stdio.h>
#include <time.h>

#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../tm/t_funcs.h"
#include "../../error.h"
#include "acc_mod.h"
#include "acc.h"
#include "../../ut.h"      /* q_memchr */

/********************************************************
 *        str_copy
 *        Copy str structure, doesn't copy
 *        the whole string !
 ********************************************************/

static inline void str_copy(str* _d, str* _s)
{
	_d->s = _s->s;
	_d->len = _s->len;
}

/********************************************************
 *        ul_fnq
 *        Find a character occurence that is not quoted
 ********************************************************/

char* ul_fnq(str* _s, char _c)
{
	int quoted = 0, i;

	for(i = 0; i < _s->len; i++) {
		if (!quoted) {
			if (_s->s[i] == '\"') quoted = 1;
			else if (_s->s[i] == _c) return _s->s + i;
		} else {
			if ((_s->s[i] == '\"') && (_s->s[i - 1] != '\\')) quoted = 0;
		}
	}
	return 0;
}

/********************************************
 *        ul_get_user
 *        Extract username part from URI
 ********************************************/

int ul_get_user(str* _s)
{
	char* at, *dcolon, *dc;
	dcolon = ul_fnq(_s, ':');

	if (dcolon == 0) return -1;

	_s->s = dcolon + 1;
	_s->len -= dcolon - _s->s + 1;

	at = q_memchr(_s->s, '@', _s->len);
	dc = q_memchr(_s->s, ':', _s->len);
	if (at) {
		if ((dc) && (dc < at)) {
			_s->len = dc - _s->s;
			return 0;
		}

		_s->len = at - _s->s;
		return 0;
	} else return -2;
}
/********************************************
 *        acc_request
 ********************************************/

int acc_request( struct sip_msg *rq, char * comment, char  *foo)
{
#ifdef SQL_ACC
    db_key_t keys[] = {mc_sip_method_col, mc_i_uri_col, mc_o_uri_col, mc_sip_from_col, mc_sip_callid_col,
                        mc_sip_to_col, mc_sip_status_col, mc_user_col, mc_time_col};
    db_val_t vals[9];

    struct tm *tm;
    time_t timep;
    char time_s[20];
    str user;
#endif

	int comment_len;

	comment_len=strlen(comment);

	/* we can parse here even if we do not knwo whether the
	   request is an incmoing pkg_mem message or stoed  shmem_msg;
	   thats because acc_missed_report operating over shmem never
	   enters acc_missed if the header fields are not parsed
	   and entering the function from script gives pkg_mem,
	   which can be parsed
	*/

	if ( parse_headers(rq, HDR_FROM | HDR_CALLID, 0)==-1
				|| !(rq->from && rq->callid) ) {
        LOG(L_ERR, "ERROR: acc_missed: From not found\n");
		return -1;
	}
    if (usesyslog) {
        LOG( log_level,
            "ACC: call missed: "
            "i-uri=%.*s, o-uri=%.*s, call_id=%.*s, "
            "from=%.*s, reason=%.*s\n",
            rq->first_line.u.request.uri.len,
            rq->first_line.u.request.uri.s,
            rq->new_uri.len, rq->new_uri.s,
            rq->callid->body.len, rq->callid->body.s,
            rq->from->body.len, rq->from->body.s,
            comment_len, comment);
    }

#ifdef SQL_ACC
    if (db_url) {
        timep = time(NULL);
        tm = gmtime(&timep);
        strftime(time_s, 20, "%Y-%m-%d %H:%M:%S", tm);

        str_copy(&user, &rq->first_line.u.request.uri);

        if ((ul_get_user(&user) < 0) || !user.len) {
            LOG(L_ERR, "ERROR: acc_request: Error while extracting username\n");
            return -1;
        }

        /* database columns:
         * "sip_method", "i_uri", "o_uri", "sip_from", "sip_callid", "sip_to", "sip_status", "user", "time"
         */

        VAL_TYPE(vals) = VAL_TYPE(vals + 1) = VAL_TYPE(vals + 2) = VAL_TYPE(vals + 3) = VAL_TYPE(vals + 4) =
                VAL_TYPE(vals + 5) = VAL_TYPE(vals + 6) = VAL_TYPE(vals + 7) = DB_STR;
        VAL_TYPE(vals + 8) = DB_STRING;

        VAL_NULL(vals) = VAL_NULL(vals + 1) = VAL_NULL(vals + 2) = VAL_NULL(vals + 3) = VAL_NULL(vals + 4) =
                VAL_NULL(vals + 5) = VAL_NULL(vals + 6) = VAL_NULL(vals + 7) = VAL_NULL(vals + 8) = 0;

        VAL_STR(vals).s     = rq->first_line.u.request.method.s;
        VAL_STR(vals).len   = rq->first_line.u.request.method.len;
        VAL_STR(vals+1).s   = rq->first_line.u.request.uri.s;
        VAL_STR(vals+1).len = rq->first_line.u.request.uri.len;
        VAL_STR(vals+2).s   = rq->new_uri.s;
        VAL_STR(vals+2).len = rq->new_uri.len;
        VAL_STR(vals+3).s   = rq->from->body.s;
        VAL_STR(vals+3).len = rq->from->body.len;
        VAL_STR(vals+4).s   = rq->callid->body.s;
        VAL_STR(vals+4).len = rq->callid->body.len;
        VAL_STR(vals+5).s   = rq->to->body.s;
        VAL_STR(vals+5).len = rq->to->body.len;
        VAL_STR(vals+6).s   = comment;
        VAL_STR(vals+6).len = comment_len;
        VAL_STR(vals+7).s   = user.s;
        VAL_STR(vals+7).len = user.len;
        VAL_STRING(vals+8)  = time_s;

        db_use_table(db_handle, db_table_mc);
        if (db_insert(db_handle, keys, vals, 9) < 0) {
            LOG(L_ERR, "ERROR: acc_request: Error while inserting to database\n");
            return -1;;
        }
    }
#endif

    return 1;
}

/********************************************
 *        acc_missed_report
 ********************************************/

void acc_missed_report( struct cell* t, struct sip_msg *reply,
	unsigned int code )
{
	struct sip_msg *rq;
	str acc_text;

	rq =  t->uas.request;
	/* it is coming from TM -- it must be already parsed ! */
	if (! rq->from ) {
		LOG(L_ERR, "ERROR: TM request for accounting not parsed\n");
		return;
	}

	get_reply_status(&acc_text, reply, code);
	if (acc_text.s==0) {
		LOG(L_ERR, "ERROR: acc_missed_report: get_reply_status failed\n" );
		return;
	}

	acc_request(rq, acc_text.s , 0 /* foo */);
	pkg_free(acc_text.s);

	/* zdravime vsechny cechy -- jestli jste se dostali se ctenim
	   kodu az sem a rozumite mu, jste na tom lip, nez autor!
	   prijmete uprimne blahoprani
	*/
}

/********************************************
 *        acc_reply_report
 ********************************************/

void acc_reply_report(  struct cell* t , struct sip_msg *reply,
	unsigned int code )
{
#ifdef SQL_ACC

    db_key_t keys[] = {acc_sip_method_col, acc_i_uri_col, acc_o_uri_col, acc_sip_from_col, acc_sip_callid_col,
                        acc_sip_to_col, acc_sip_status_col, acc_user_col, acc_time_col};
    db_val_t vals[9];

    struct tm *tm;
    time_t timep;
    char time_s[20];
    str user;
#endif

	struct sip_msg *rq;

	/* note that we don't worry about *reply -- it may be actually
	   a pointer FAKED_REPLY and we need only code anyway, anything else is
	   stored in transaction context
	*/
	rq =  t->uas.request;

	/* take call-if from TM -- it is parsed for requests and we do not
	   want to parse it from reply unnecessarily */
	/* don't try to parse any more -- the request is conserved in shmem */
	if ( /*parse_headers(rq, HDR_CALLID)==-1 || */
		!  (rq->callid && rq->from )) {
		LOG(L_INFO, "ERROR: attempt to account on a reply to request "
			"with an invalid Call-ID or From\n");
		return;
	}
    if (usesyslog) {
        LOG( log_level,
            "ACC: transaction answered: "
            "method=%.*s, i-uri=%.*s, o-uri=%.*s, call_id=%.*s, "
            "from=%.*s, code=%d\n",
            rq->first_line.u.request.method.len,
            rq->first_line.u.request.method.s,
            rq->first_line.u.request.uri.len,
            rq->first_line.u.request.uri.s,
            rq->new_uri.len, rq->new_uri.s,
            rq->callid->body.len, rq->callid->body.s,
            rq->from->body.len, rq->from->body.s,
            /* take a reply from message -- that's safe and we don't need to be
            worried about TM reply status being changed concurrently */
            code );
    }

#ifdef SQL_ACC
    if (db_url) {
        timep = time(NULL);
        tm = gmtime(&timep);
        strftime(time_s, 20, "%Y-%m-%d %H:%M:%S", tm);

        str_copy(&user, &rq->first_line.u.request.uri);

        if ((ul_get_user(&user) < 0) || !user.len) {
            LOG(L_ERR, "ERROR: acc_reply_report: Error while extracting username\n");
            return;
        }

        /* database columns:
         * "sip_method", "i_uri", "o_uri", "sip_from", "sip_callid", "sip_to", "sip_status", "user", "time"
         */

        VAL_TYPE(vals) = VAL_TYPE(vals + 1) = VAL_TYPE(vals + 2) = VAL_TYPE(vals + 3) = VAL_TYPE(vals + 4) =
                VAL_TYPE(vals + 5) = DB_STR;
        VAL_TYPE(vals + 6) = DB_INT;
        VAL_TYPE(vals + 7) = DB_STR;
        VAL_TYPE(vals + 8) = DB_STRING;

        VAL_NULL(vals) = VAL_NULL(vals + 1) = VAL_NULL(vals + 2) = VAL_NULL(vals + 3) = VAL_NULL(vals + 4) =
                VAL_NULL(vals + 5) = VAL_NULL(vals + 6) = VAL_NULL(vals + 7) = VAL_NULL(vals + 8) = 0;

        VAL_STR(vals).s     = rq->first_line.u.request.method.s;
        VAL_STR(vals).len   = rq->first_line.u.request.method.len;
        VAL_STR(vals+1).s   = rq->first_line.u.request.uri.s;
        VAL_STR(vals+1).len = rq->first_line.u.request.uri.len;
        VAL_STR(vals+2).s   = rq->new_uri.s;
        VAL_STR(vals+2).len = rq->new_uri.len;
        VAL_STR(vals+3).s   = rq->from->body.s;
        VAL_STR(vals+3).len = rq->from->body.len;
        VAL_STR(vals+4).s   = rq->callid->body.s;
        VAL_STR(vals+4).len = rq->callid->body.len;
        VAL_STR(vals+5).s   = rq->to->body.s;
        VAL_STR(vals+5).len = rq->to->body.len;
        VAL_INT(vals+6)     = code;
        VAL_STR(vals+7).s   = user.s;
        VAL_STR(vals+7).len = user.len;
        VAL_STRING(vals+8)  = time_s;

        db_use_table(db_handle, db_table_acc);
        if (db_insert(db_handle, keys, vals, 9) < 0) {
            LOG(L_ERR, "ERROR: acc_reply_report: Error while inserting to database\n");
            return;
        }
    }
#endif
}

/********************************************
 *        acc_ack_report
 ********************************************/

void acc_ack_report(  struct cell* t , struct sip_msg *ack )
{

#ifdef SQL_ACC
    db_key_t keys[] = {acc_sip_method_col, acc_i_uri_col, acc_o_uri_col, acc_sip_from_col, acc_sip_callid_col,
                        acc_sip_to_col, acc_sip_status_col, acc_user_col, acc_time_col};
    db_val_t vals[9];

    struct tm *tm;
    time_t timep;
    char time_s[20];
    str user;
#endif

    struct sip_msg *rq;

	rq =  t->uas.request;

	if (/* parse_headers(rq, HDR_CALLID, 0)==-1 || */
			!( rq->callid && rq->from )) {
		LOG(L_INFO, "ERROR: attempt to account on a request with invalid Call-ID\n");
		return;
	}
    if (usesyslog) {
        LOG( log_level, "ACC: transaction acknowledged: "
            "method=%.*s, i-uri=%.*s, o-uri=%.*s, call_id=%.*s,"
            "from=%.*s, code=%d\n",
            /* take all stuff directly from message */

            /* CAUTION -- need to re-think here !!! The issue is with magic cookie
            transaction matching all this stuff does not have to be parsed; so I
            may want to try parsing here; which raises all the annoying issues
            about in which memory space the parsed structures will live
            */
            ack->first_line.u.request.method.len, ack->first_line.u.request.method.s,
            ack->first_line.u.request.uri.len, ack->first_line.u.request.uri.s,
            ack->new_uri.len, ack->new_uri.s,
            ack->callid->body.len, ack->callid->body.s,
            ack->from->body.len, ack->from->body.s,
            t->uas.status  );

    }

#ifdef SQL_ACC
    if (db_url) {
        timep = time(NULL);
        tm = gmtime(&timep);
        strftime(time_s, 20, "%Y-%m-%d %H:%M:%S", tm);

        str_copy(&user, &ack->first_line.u.request.uri);

        if ((ul_get_user(&user) < 0) || !user.len) {
            LOG(L_ERR, "ERROR: acc_ack_report: Error while extracting username\n");
            return;
        }

        /* database columns:
         * "sip_method", "i_uri", "o_uri", "sip_from", "sip_callid", "sip_to", "sip_status", "user", "time"
         */

        VAL_TYPE(vals) = VAL_TYPE(vals + 1) = VAL_TYPE(vals + 2) = VAL_TYPE(vals + 3) = VAL_TYPE(vals + 4) =
                VAL_TYPE(vals + 5) = DB_STR;
        VAL_TYPE(vals + 6) = DB_INT;
        VAL_TYPE(vals + 7) = DB_STR;
        VAL_TYPE(vals + 8) = DB_STRING;

        VAL_NULL(vals) = VAL_NULL(vals + 1) = VAL_NULL(vals + 2) = VAL_NULL(vals + 3) = VAL_NULL(vals + 4) =
                VAL_NULL(vals + 5) = VAL_NULL(vals + 6) = VAL_NULL(vals + 7) = VAL_NULL(vals + 8) = 0;

        VAL_STR(vals).s     = ack->first_line.u.request.method.s;
        VAL_STR(vals).len   = ack->first_line.u.request.method.len;
        VAL_STR(vals+1).s   = ack->first_line.u.request.uri.s;
        VAL_STR(vals+1).len = ack->first_line.u.request.uri.len;
        VAL_STR(vals+2).s   = ack->new_uri.s;
        VAL_STR(vals+2).len = ack->new_uri.len;
        VAL_STR(vals+3).s   = ack->from->body.s;
        VAL_STR(vals+3).len = ack->from->body.len;
        VAL_STR(vals+4).s   = ack->callid->body.s;
        VAL_STR(vals+4).len = ack->callid->body.len;
        VAL_STR(vals+5).s   = ack->to->body.s;
        VAL_STR(vals+5).len = strlen(ack->to->body.s);
//        VAL_STR(vals+5).len = ack->to->body.len;
        VAL_INT(vals+6)     = t->uas.status;
        VAL_STR(vals+7).s   = user.s;
        VAL_STR(vals+7).len = user.len;
        VAL_STRING(vals+8)  = time_s;

        db_use_table(db_handle, db_table_acc);
        if (db_insert(db_handle, keys, vals, 9) < 0) {
            LOG(L_ERR, "ERROR: acc_ack_report: Error while inserting to database\n");
            return;
        }
    }
#endif
}
