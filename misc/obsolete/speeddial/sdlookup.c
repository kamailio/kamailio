/*
 * $Id$
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * SPEEDDIAL SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * SPEEDDIAL SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *  
 *
 * History:
 * ---------
 * 
 */


#include <string.h>

#include "../../dprint.h"
#include "../../action.h"
#include "../../config.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../lib/srdb2/db.h"
#include "../../id.h"
#include "../../dset.h"

#include "speeddial.h"
#include "sdlookup.h"

#define MAX_USERURI_SIZE	256

static char useruri_buf[MAX_USERURI_SIZE];


/**
 *
 */
int sd_lookup(struct sip_msg* _msg, char* _index, char* _str2)
{
	int i;
	str user_s, uid, did;
	db_res_t* res = NULL;
	db_rec_t* rec;

	/* init */
	i = (int)(long)_index;

	     /* Retrieve the owner of the record */
	if (get_from_uid(&uid, _msg) < 0) {
		LOG(L_ERR, "sd_lookup: Unable to get user identity\n");
		return -1;
	}

	     /* Retrieve the called domain id */
	if (get_to_did(&did, _msg) < 0) {
		LOG(L_ERR, "sd_lookup: Destination domain ID not known\n");
		return -1;
	}

	tables[i].lookup_num->match[0].v.lstr = uid;
	tables[i].lookup_num->match[1].v.lstr = did;

	/* Get the called username */
	if (parse_sip_msg_uri(_msg) < 0)
	{
		LOG(L_ERR, "sd_lookup: Error while parsing Request-URI\n");
		goto err_badreq;
	}
	
	tables[i].lookup_num->match[2].v.lstr = _msg->parsed_uri.user;

	DBG("speeddial: Looking up (uid:%.*s,username:%.*s,did:%.*s)\n",
			uid.len, uid.s,
	    _msg->parsed_uri.user.len, _msg->parsed_uri.user.s,
	    did.len, did.s);
	
	if (db_exec(&res, tables[i].lookup_num) < 0) {
		ERR("speeddial: Error while executing database command\n");
		goto err_server;
	}

	if (res == NULL) {
		DBG("speeddial: No SIP URI found for speeddial (num:%.*s, uid:%.*s,"
			" did:%.*s)\n",
			_msg->parsed_uri.user.len,
			_msg->parsed_uri.user.s,
			uid.len, uid.s,
			did.len, did.s);
		return -1;
	}

	user_s.s = useruri_buf + 4;
	rec = db_first(res);
	while(rec) {
		if (rec->fld[0].flags & DB_NULL) goto skip;
		strncpy(user_s.s, 
				rec->fld[0].v.lstr.s,
				rec->fld[0].v.lstr.len);
		user_s.len = rec->fld[0].v.lstr.len;
		user_s.s[user_s.len] = '\0';
		goto out;

	skip:
		rec = db_next(res);
	}

	if (rec == NULL) {
		DBG("speeddial: No usable SIP URI found for (num:%.*s, uid:%.*s,"
			" did:%.*s)\n",
			_msg->parsed_uri.user.len,
			_msg->parsed_uri.user.s,
			uid.len, uid.s,
			did.len, did.s);
		db_res_free(res);
		return -1;
	}

 out:
	/* check 'sip:' */
	if(user_s.len<4 || strncmp(user_s.s, "sip:", 4)) {
		memcpy(useruri_buf, "sip:", 4);
		user_s.s -= 4;
		user_s.len += 4;
	}

	db_res_free(res);

	/* set the URI */
	DBG("sd_lookup: URI of sd from R-URI [%s]\n", user_s.s);
	if(rewrite_uri(_msg, &user_s)<0)
	{
		LOG(L_ERR, "sd_lookup: Cannot replace the R-URI\n");
		goto err_server;
	}

	return 1;

err_server:
	if (slb.zreply(_msg, 500, "Server Internal Error") == -1)
	{
		LOG(L_ERR, "sd_lookup: Error while sending reply\n");
	}
	return 0;
err_badreq:
	if (slb.zreply(_msg, 400, "Bad Request") == -1)
	{
		LOG(L_ERR, "sd_lookup: Error while sending reply\n");
	}
	return 0;
}
