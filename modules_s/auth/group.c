/*
 * $Id$
 *
 * Checks if a username matche those in digest credentials
 * or is member of a group
 */


#include "group.h"
#include <string.h>
#include "../../dprint.h"
#include "../../db/db.h"
#include "auth_mod.h"                   /* Module parameters */
#include "../../parser/digest/digest.h" /* get_authorized_cred */


/*
 * Check if the username matches the username in credentials
 */
int is_user(struct sip_msg* _msg, char* _user, char* _str2)
{
	str* s;
	struct hdr_field* h;
	auth_body_t* c;

	s = (str*)_user;

	get_authorized_cred(_msg->authorization, &h);
	if (!h) {
		get_authorized_cred(_msg->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "is_user(): No authorized credentials found (error in scripts)\n");
			return -1;
		}
	}

	c = (auth_body_t*)(h->parsed);

	if (!c->digest.username.len) {
		DBG("is_user(): Username not found in credentials\n");
		return -1;
	}

	if (s->len != c->digest.username.len) {
		return -1;
	}

	if (!memcmp(s->s, c->digest.username.s, s->len)) {
		DBG("is_user(): Username matches\n");
		return 1;
	} else {
		DBG("is_user(): Username differs\n");
		return -1;
	}
}


/*
 * Check if the user specified in credentials is a member
 * of given group
 */
int is_in_group(struct sip_msg* _msg, char* _group, char* _str2)
{
	db_key_t keys[] = {grp_user_col, grp_grp_col};
	db_val_t vals[2];
	db_key_t col[] = {grp_grp_col};
	db_res_t* res;
	struct hdr_field* h;
	auth_body_t* c;

	get_authorized_cred(_msg->authorization, &h);
	if (!h) {
		get_authorized_cred(_msg->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "is_in_group(): No authorized credentials found (error in scripts)\n");
			return -1;
		}
	}

	c = (auth_body_t*)(h->parsed);

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;

	VAL_STR(vals).s = c->digest.username.s;
	VAL_STR(vals).len = c->digest.username.len;
	
	VAL_STR(vals + 1).s = ((str*)_group)->s;
	VAL_STR(vals + 1).len = ((str*)_group)->len;
	
	db_use_table(db_handle, grp_table);
	if (db_query(db_handle, keys, vals, col, 2, 1, NULL, &res) < 0) {
		LOG(L_ERR, "is_in_group(): Error while querying database\n");
		return -1;
	}
	
	if (RES_ROW_N(res) == 0) {
		DBG("is_in_group(): User %s is not in group %s\n", c->digest.username.s, _group);
		db_free_query(db_handle, res);
		return -1;
	} else {
		DBG("is_in_group(): User %s is member of group %s\n", c->digest.username.s, _group);
		db_free_query(db_handle, res);
		return 1;
	}
}


/*
 * Check if user specified in credentials is in a table
 */
int is_user_in(struct sip_msg* _msg, char* _table, char* _s)
{
	db_key_t key = grp_user_col;
	db_val_t val;
	db_key_t col = grp_user_col;
	db_res_t* res;

	struct hdr_field* h;
	auth_body_t* c;

	get_authorized_cred(_msg->authorization, &h);
	if (!h) {
		get_authorized_cred(_msg->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "is_user_in(): No authorized credentials found (error in scripts)\n");
			return -1;
		}
	}

	c = (auth_body_t*)(h->parsed);	

	VAL_TYPE(&val) = DB_STR;
	VAL_NULL(&val) = 0;
	VAL_STR(&val).s = ((str*)_table)->s;
	VAL_STR(&val).len = ((str*)_table)->len;

	db_use_table(db_handle, grp_table);

	if (db_query(db_handle, &key, &val, &col, 1, 1, NULL, &res) < 0) {
		LOG(L_ERR, "is_user_in(): Error while querying database\n");
		return -1;
	}
	
	if (RES_ROW_N(res) == 0) {
		db_free_query(db_handle, res);
		return -1;
	} else {
		db_free_query(db_handle, res);
		return 1;
	}
}
