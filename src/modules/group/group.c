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
 *
 */

/**
 * \file
 * \brief Group membership module
 * \ingroup group
 * - Module: \ref group
 */


#include <string.h>
#include "../../dprint.h"               /* Logging */
#include "../../lib/srdb1/db.h"                /* Generic database API */
#include "../../ut.h"
#include "../../parser/digest/digest.h" /* get_authorized_cred */
#include "../../parser/hf.h"            /* Header Field types */
#include "../../parser/parse_from.h"    /* From parser */
#include "../../parser/parse_uri.h"
#include "group.h"
#include "group_mod.h"                   /* Module parameters */

/*!
 * \brief Extract the username and domain from the SIP message
 *
 * Set the username and domain depending on the value of the SIP
 * message and the group check structure.
 * \param msg SIP message
 * \param gcp group check structure
 * \param username stored username
 * \param domain stored domain
 * \return 0 on success, -1 on failure
 */
int get_username_domain(struct sip_msg *msg, group_check_p gcp,
											str *username, str *domain)
{
	struct sip_uri puri;
	struct sip_uri *turi;
	struct hdr_field* h;
	struct auth_body* c = 0;
	pv_value_t value;

	turi = NULL;

	switch(gcp->id) {
		case 1: /* Request-URI */
			if(parse_sip_msg_uri(msg)<0) {
				LM_ERR("failed to get Request-URI\n");
				return -1;
			}
			turi = &msg->parsed_uri;
			break;

		case 2: /* To */
			if((turi=parse_to_uri(msg))==NULL) {
				LM_ERR("failed to get To URI\n");
				return -1;
			}
			break;

		case 3: /* From */
			if((turi=parse_from_uri(msg))==NULL) {
				LM_ERR("failed to get From URI\n");
				return -1;
			}
			break;

		case 4: /* Credentials */
			get_authorized_cred( msg->authorization, &h);
			if (!h) {
				get_authorized_cred( msg->proxy_auth, &h);
				if (!h) {
					LM_ERR("no authorized credentials found "
							"(error in scripts)\n");
					return -1;
				}
			}
			c = (auth_body_t*)(h->parsed);
			break;

		case 5: /* AVP spec */
			if(pv_get_spec_value( msg, &gcp->sp, &value)!=0 
				|| value.flags&PV_VAL_NULL || value.rs.len<=0)
			{
				LM_ERR("no AVP found (error in scripts)\n");
				return -1;
			}
			if (parse_uri(value.rs.s, value.rs.len, &puri) < 0) {
				LM_ERR("failed to parse URI <%.*s>\n",value.rs.len, value.rs.s);
				return -1;
			}
			turi = &puri;
			break;
		default: {
			LM_ERR("incorrect check id %d\n", gcp->id);
			return -1;
		}
	}

	if (gcp->id != 4) {
		*username = turi->user;
		*domain = turi->host;
	} else {
		*username = c->digest.username.user;
		*domain = *(GET_REALM(&c->digest));
	}
	return 0;
}



/*!
 * \brief Check if username in specified header field is in a table
 * \param _msg SIP message
 * \param _hf Header field
 * \param _grp checked table
 * \return 1 on success, negative on failure 
 */
int is_user_in(struct sip_msg* _msg, char* _hf, char* _grp)
{
	db_key_t keys[3];
	db_val_t vals[3];
	db_key_t col[1];
	db1_res_t* res = NULL;

	keys[0] = &user_column;
	keys[1] = &group_column;
	keys[2] = &domain_column;
	col[0]  = &group_column;

	if ( get_username_domain( _msg, (group_check_p)_hf, &(VAL_STR(vals)),
	&(VAL_STR(vals+2)))!=0) {
		LM_ERR("failed to get username@domain\n");
		return -1;
	}

	if (VAL_STR(vals).s==NULL || VAL_STR(vals).len==0 ) {
		LM_DBG("no username part\n");
		return -1;
	}

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = VAL_TYPE(vals + 2) = DB1_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = VAL_NULL(vals + 2) = 0;

	VAL_STR(vals + 1) = *((str*)_grp);

	if (group_dbf.use_table(group_dbh, &table) < 0) {
		LM_ERR("failed to use_table\n");
		return -5;
	}

	if (group_dbf.query(group_dbh, keys, 0, vals, col, (use_domain) ? (3): (2),
				1, 0, &res) < 0) {
		LM_ERR("failed to query database\n");
		return -5;
	}

	if (RES_ROW_N(res) == 0) {
		LM_DBG("user is not in group '%.*s'\n", 
		    ((str*)_grp)->len, ZSW(((str*)_grp)->s));
		group_dbf.free_result(group_dbh, res);
		return -6;
	} else {
		LM_DBG("user is in group '%.*s'\n", 
			((str*)_grp)->len, ZSW(((str*)_grp)->s));
		group_dbf.free_result(group_dbh, res);
		return 1;
	}
}


/*!
 * \brief Initialize the DB connection
 * \param db_url database URL
 * \return 0 on success, -1 on failure
 */
int group_db_init(const str* db_url)
{
	if (group_dbf.init==0){
		LM_CRIT("null dbf \n");
		goto error;
	}
	group_dbh=group_dbf.init(db_url);
	if (group_dbh==0){
		LM_ERR("unable to connect to the database\n");
		goto error;
	}
	return 0;
error:
	return -1;
}


/*!
 * \brief Bind the DB connection
 * \param db_url database URL
 * \return 0 on success, -1 on failure
 */
int group_db_bind(const str* db_url)
{
	if (db_bind_mod(db_url, &group_dbf)<0){
		LM_ERR("unable to bind to the database module\n");
		return -1;
	}

	if (!DB_CAPABILITY(group_dbf, DB_CAP_QUERY)) {
		LM_ERR("database module does not implement 'query' function\n");
		return -1;
	}

	return 0;
}


/*!
 * \brief Close the DB connection
 */
void group_db_close(void)
{
	if (group_dbh && group_dbf.close){
		group_dbf.close(group_dbh);
		group_dbh=0;
	}
}
