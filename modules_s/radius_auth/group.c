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
#include "../../parser/hf.h"
#include "../../parser/parse_from.h"
#include "common.h"
#include <radiusclient.h>
#include "utils.h"

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
 * This is an alternative version that works with the implementation
 * provided by freeradius. The difference here is that all the parameters
 * are placed into one Attribute (DIGEST_ATTRIBUTES) so that to economize
 * on name-mapping on the radius servers. I have kept the code structure
 * similar to the previous example and have adjusted to DIGEST_ATTRIBUTES
 * prior to sending the msg for code simplicity.
 */
char* radius_get_group(dig_cred_t * cred, str* _method) 
{
	int             result;
    char            msg[4096];
    VALUE_PAIR      *send, *received;
    UINT4           service;
    VALUE_PAIR 		*vp;    
	str				method;
	char*			group;

	//group = NULL;
	send = NULL;
	
	method.s = _method->s;
	method.len = _method->len;

	/*
	 * Add all the user digest parameters according to the qop defined.
	 * Most devices tested only offer support for the simplest digest.
	 */
	if (rc_avpair_add(&send, PW_USER_NAME, 
						cleanbody(cred->username), 0) == NULL)
    	return(NULL);

	if (rc_avpair_add(&send, PW_DIGEST_USER_NAME, 
						cleanbody(cred->username), 0) == NULL)
    	return (NULL);

	if (rc_avpair_add(&send, PW_DIGEST_REALM, 
						cleanbody(cred->realm), 0) == NULL)
        return (NULL);
 
	if (rc_avpair_add(&send, PW_DIGEST_NONCE, 
						cleanbody(cred->nonce), 0) == NULL)
        return (NULL);
 
	if (rc_avpair_add(&send, PW_DIGEST_URI, 
						cleanbody(cred->uri), 0) == NULL)
        return (NULL);
	
	if (rc_avpair_add(&send, PW_DIGEST_METHOD, 
						cleanbody(method), 0) == NULL)
        return (NULL);
	
	/* 
	 * Add the additional authentication fields according to the QOP.
	 */
	if (cred->qop.qop_parsed == QOP_AUTH) {
		if (rc_avpair_add(&send, PW_DIGEST_QOP, "auth", 0) == NULL) {
        	return (NULL);
		}
		
		if (rc_avpair_add(&send, PW_DIGEST_NONCE, 
							cleanbody(cred->nc), 0) == NULL)
        return (NULL);
		
		if (rc_avpair_add(&send, PW_DIGEST_CNONCE, 
							cleanbody(cred->cnonce), 0) == NULL) {
        	return (NULL);
		}
		
	} else if (cred->qop.qop_parsed == QOP_AUTHINT) {
		if (rc_avpair_add(&send, PW_DIGEST_QOP, "auth-int", 0) == NULL)
        	return (NULL);

		if (rc_avpair_add(&send, PW_DIGEST_NONCE_COUNT, 
							cleanbody(cred->nc), 0) == NULL) {
			return (NULL);
		}
		
		if (rc_avpair_add(&send, PW_DIGEST_CNONCE, 
							cleanbody(cred->cnonce), 0) == NULL) {
        	return (NULL);
		}

		if (rc_avpair_add(&send, PW_DIGEST_BODY_DIGEST, 
							cleanbody(cred->opaque), 0) == NULL) {
        	return (NULL);
		}
		
	} else  {
		/* send nothing for qop == "" */
	}
	
	

	/*
	 * Now put everything place all the previous attributes into the
	 * PW_DIGEST_ATTRIBUTES
	 */
	
	/*
	 *  Fix up Digest-Attributes issues see draft-sterman-aaa-sip-00
	 */
	for (vp = send; vp != NULL; vp = vp->next) {
		switch (vp->attribute) {
	  		default:
	    	break;

			/* Fall thru the know values */
			case PW_DIGEST_REALM:
			case PW_DIGEST_NONCE:
			case PW_DIGEST_METHOD:
			case PW_DIGEST_URI:
			case PW_DIGEST_QOP:
			case PW_DIGEST_ALGORITHM:
			case PW_DIGEST_BODY_DIGEST:
			case PW_DIGEST_CNONCE:
			case PW_DIGEST_NONCE_COUNT:
			case PW_DIGEST_USER_NAME:
	
			/* overlapping! */
			memmove(&vp->strvalue[2], &vp->strvalue[0], vp->lvalue);
			vp->strvalue[0] = vp->attribute - PW_DIGEST_REALM + 1;
			vp->lvalue += 2;
			vp->strvalue[1] = vp->lvalue;
			vp->attribute = PW_DIGEST_ATTRIBUTES;
			break;
		}
	}

	/* Add the response... What to calculate against... */
	if (rc_avpair_add(&send, PW_DIGEST_RESPONSE, 
						cleanbody(cred->response), 0) == NULL)
        return (NULL);

	/* Indicate the service type, Authenticate only in our case */
       service = PW_AUTHENTICATE_ONLY;
	if (rc_avpair_add(&send, PW_SERVICE_TYPE, &service, 0) == NULL) {
		DBG("radius_authorize() Error adding service type \n");
	 	return (NULL);  	
	}
       
    result = rc_auth(0, send, &received, msg);
       
    if (result == OK_RC) {
    	DBG("RADIUS AUTHENTICATION SUCCESS \n");
		/*TODO:vp_printlist*/
		if (msg != NULL) {
			DBG("You belong to group: %s \n", msg);
			group = &msg[0];
		}
	} else {
		DBG("RADIUS AUTHENTICATION FAILURE \n");
	}
    return group;
}

int radius_is_in_group(struct sip_msg* msg, char* _group) {
	char* 			group;
	auth_body_t * 	cred;
	
	/* Extract credentials */
	if (!(msg->authorization)) {
		     /* No credentials parsed yet */
		if (parse_headers(msg, HDR_AUTHORIZATION, 0) == -1) {
			LOG(L_ERR, "radis_log_reply: Error while parsing auth headers\n");
			return -2;
		}
	}
	
	/* Parse only if there's something there... */
	if (msg->authorization) {
		if (parse_credentials(msg->authorization) != -1) {
			cred = (auth_body_t*)(msg->authorization->parsed);
			if (check_dig_cred(&(cred->digest)) != E_DIG_OK) {
				LOG(L_ERR, "radius_log_reply: Credentials missing\n");
				return(-1);
			} 
		}
	} 

	group = radius_get_group(&(cred->digest), 
					&(msg->first_line.u.request.method));
	if (group) {
		if (strncmp(_group, group, strlen(_group)) == 0) {
			DBG("User is in group %s\n", group);
			return(1);
		} else {
			DBG("User in not part of group %s\n", group);
			return(-1);
		}
	} else {
		return(-1);
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
	if (db_query(db_handle, keys, vals, col, 2, 1, 0, &res) < 0) {
		LOG(L_ERR, "is_in_group(): Error while querying database\n");
		return -1;
	}
	
	if (RES_ROW_N(res) == 0) {
		DBG("is_in_group(): User \'%.*s\' is not in group \'%.*s\'\n", 
		    c->digest.username.len, c->digest.username.s,
		    ((str*)_group)->len, ((str*)_group)->s);
		db_free_query(db_handle, res);
		return -1;
	} else {
		DBG("is_in_group(): User \'%.*s\' is member of group \'%.*s\'\n", 
		    c->digest.username.len, c->digest.username.s,
		    ((str*)_group)->len, ((str*)_group)->s);
		db_free_query(db_handle, res);
		return 1;
	}
}


/*
 * Extract username from Request-URI
 */
static inline int get_request_user(struct sip_msg* _m, str* _s)
{
	if (_m->new_uri.s) {
		_s->s = _m->new_uri.s;
		_s->len = _m->new_uri.len;
	} else {
		_s->s = _m->first_line.u.request.uri.s;
		_s->len = _m->first_line.u.request.uri.len;
	}
	if (auth_get_username(_s) < 0) {
		LOG(L_ERR, "get_request_user(): Error while extracting username\n");
		return -1;
	}
	return 0;
}


/*
 * Extract username from To header field
 */
static inline int get_to_user(struct sip_msg* _m, str* _s)
{
	if (!_m->to && (parse_headers(_m, HDR_TO, 0) == -1)) {
		LOG(L_ERR, "is_user_in(): Error while parsing message\n");
		return -1;
	}
	if (!_m->to) {
		LOG(L_ERR, "is_user_in(): To HF not found\n");
		return -2;
	}
	
	_s->s = ((struct to_body*)_m->to->parsed)->uri.s;
	_s->len = ((struct to_body*)_m->to->parsed)->uri.len;

	if (auth_get_username(_s) < 0) {
		LOG(L_ERR, "get_to_user(): Error while extracting username\n");
		return -3;
	}
	return 0;
}


/*
 * Extract username from From header field
 */
static inline int get_from_user(struct sip_msg* _m, str* _s)
{
	if (!_m->from && (parse_headers(_m, HDR_FROM, 0) == -1)) {
		LOG(L_ERR, "is_user_in(): Error while parsing message\n");
		return -3;
	}
	if (!_m->from) {
		LOG(L_ERR, "is_user_in(): From HF not found\n");
		return -4;
	}
	
	if (parse_from_header(_m->from) < 0) {
		LOG(L_ERR, "is_user_in(): Error while parsing From body\n");
		return -5;
	}
	
	_s->s = ((struct to_body*)_m->from->parsed)->uri.s;
	_s->len = ((struct to_body*)_m->from->parsed)->uri.len;

	if (auth_get_username(_s) < 0) {
		LOG(L_ERR, "is_user_in(): Error while extracting username\n");
		return -6;
	}

	return 0;
}


/*
 * Extract username from digest credentials
 */
static inline int get_cred_user(struct sip_msg* _m, str* _s)
{
	struct hdr_field* h;
	auth_body_t* c;
	
	get_authorized_cred(_m->authorization, &h);
	if (!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "is_user_in(): No authorized credentials found (error in scripts)\n");
			return -6;
		}
	}
	
	c = (auth_body_t*)(h->parsed);

	_s->s = c->digest.username.s;
	_s->len = c->digest.username.len;

	return 0;
}


/*
 * Check if username in specified header field is in a table
 */
int is_user_in(struct sip_msg* _msg, char* _hf, char* _grp)
{
	db_key_t keys[] = {grp_user_col, grp_grp_col};
	db_val_t vals[2];
	db_key_t col[1] = {grp_grp_col};
	db_res_t* res;
	str user;

	switch((int)_hf) {
	case 1: /* Request-URI */
		if (get_request_user(_msg, &user) < 0) {
			LOG(L_ERR, "is_user_in(): Error while obtaining username from Request-URI\n");
			return -1;
		}
		break;

	case 2: /* To */
		if (get_to_user(_msg, &user) < 0) {
			LOG(L_ERR, "is_user_in(): Error while extracting To username\n");
			return -2;
		}
		break;

	case 3: /* From */
		if (get_from_user(_msg, &user) < 0) {
			LOG(L_ERR, "is_user_in(): Error while extracting From username\n");
			return -3;
		}
		break;

	case 4: /* Credentials */
		if (get_cred_user(_msg, &user) < 0) {
			LOG(L_ERR, "is_user_in(): Error while extracting digest username\n");
			return -4;
		}
		break;
	}

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;
	
	VAL_STR(vals).s = user.s;
	VAL_STR(vals).len = user.len;

	VAL_STR(vals + 1).s = ((str*)_grp)->s;
	VAL_STR(vals + 1).len = ((str*)_grp)->len;
	
	db_use_table(db_handle, grp_table);
	if (db_query(db_handle, keys, vals, col, 2, 1, 0, &res) < 0) {
		LOG(L_ERR, "is_user_in(): Error while querying database\n");
		return -5;
	}
	
	if (RES_ROW_N(res) == 0) {
		DBG("is_user_in(): User \'%.*s\' is not in group \'%.*s\'\n", 
		    user.len, user.s,
		    ((str*)_grp)->len, ((str*)_grp)->s);
		db_free_query(db_handle, res);
		return -6;
	} else {
		DBG("is_user(): User \'%.*s\' is in table \'%.*s\'\n", 
		    user.len, user.s,
		    ((str*)_grp)->len, ((str*)_grp)->s);
		db_free_query(db_handle, res);
		return 1;
	}
}
