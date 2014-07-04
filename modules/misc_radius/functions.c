/*
 * functions.c  Implementation of exported functions
 *
 * Copyright (C) 2004 FhG Fokus
 * Copyright (C) 2008 Juha Heinanen <jh@tutpro.com>
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

#include "../../mod_fix.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../usr_avp.h"
#include "../../lib/kcore/radius.h"
#include "../../parser/parse_uri.h"
#include "misc_radius.h"
#include "extra.h"

/* Array for extra attribute values */
static str val_arr[MAX_EXTRA];

/* Extract one reply item value to AVP flags, name and value */
static inline int extract_avp(VALUE_PAIR* vp, unsigned short *flags,
		int_str *name, int_str *value)
{
	static str names, values;
	unsigned int r;
	char *p;
	char *end;

	/* empty? */
	if (vp->lvalue==0 || vp->strvalue==0)
		goto error;

	p = vp->strvalue;
	end = vp->strvalue + vp->lvalue;

	/* get name */
	if (*p!='#') {
		/* name AVP */
		*flags |= AVP_NAME_STR;
		names.s = p;
	} else {
		names.s = ++p;
	}

	names.len = 0;
	while( p<end && *p!=':' && *p!='#')
		p++;
	if (names.s==p || p==end) {
		LM_ERR("empty AVP name\n");
		goto error;
	}
	names.len = p - names.s;

	/* get value */
	if (*p!='#') {
		/* string value */
		*flags |= AVP_VAL_STR;
	}
	values.s = ++p;
	values.len = end-values.s;
	if (values.len==0) {
		LM_ERR("empty AVP value\n");
		goto error;
	}

	if ( !((*flags)&AVP_NAME_STR) ) {
		/* convert name to id*/
		if (str2int(&names,&r)!=0 ) {
			LM_ERR("invalid AVP ID '%.*s'\n", names.len,names.s);
			goto error;
		}
		name->n = (int)r;
	} else {
		name->s = names;
	}

	if ( !((*flags)&AVP_VAL_STR) ) {
		/* convert value to integer */
		if (str2int(&values,&r)!=0 ) {
			LM_ERR("invalid AVP numrical value '%.*s'\n", values.len,values.s);
			goto error;
		}
		value->n = (int)r;
	} else {
		value->s = values;
	}

	return 0;
error:
	return -1;
}

static void generate_avps_rad(VALUE_PAIR* received)
{
	int_str name, val;
	unsigned short flags;
	VALUE_PAIR *vp;

	vp = received;

	for( ; vp ; vp=vp->next) {
		flags = AVP_NAME_STR;
		switch(vp->type)
		{
			case PW_TYPE_STRING:
				flags |= AVP_VAL_STR;
				name.s.len = strlen(vp->name);
				val.s.len = strlen(vp->strvalue);
				name.s.s = vp->name;
				val.s.s = vp->strvalue;
				if (add_avp( flags, name, val ) < 0) {
					LM_ERR("unable to create a new AVP\n");
				} else {
					LM_DBG("AVP '%.*s'/%d='%.*s'/%d has been added\n",
							(flags&AVP_NAME_STR)?name.s.len:4,
							(flags&AVP_NAME_STR)?name.s.s:"null",
							(flags&AVP_NAME_STR)?0:name.n,
							(flags&AVP_VAL_STR)?val.s.len:4,
							(flags&AVP_VAL_STR)?val.s.s:"null",
							(flags&AVP_VAL_STR)?0:val.n );
				}
				continue;
			case PW_TYPE_INTEGER:
				name.s.len = strlen(vp->name);
				name.s.s = vp->name;
				val.n = vp->lvalue;
				if (add_avp( flags, name, val ) < 0) {
					LM_ERR("unable to create a new AVP\n");
				} else {
					LM_DBG("AVP '%.*s'/%d='%.*s'/%d has been added\n",
							(flags&AVP_NAME_STR)?name.s.len:4,
							(flags&AVP_NAME_STR)?name.s.s:"null",
							(flags&AVP_NAME_STR)?0:name.n,
							(flags&AVP_VAL_STR)?val.s.len:4,
							(flags&AVP_VAL_STR)?val.s.s:"null",
							(flags&AVP_VAL_STR)?0:val.n );
				}
				continue;
			default:
				LM_ERR("skip attribute type %d (non-string)", vp->type);
				continue;
		}
		return;
	}
}


/* Generate AVPs from Radius reply items */
static void generate_avps(struct attr *attrs, VALUE_PAIR* received)
{
	int_str name, val;
	unsigned short flags;
	VALUE_PAIR *vp;

	vp = received;

	for( ; (vp=rc_avpair_get(vp,attrs[SA_SIP_AVP].v,0)) ; vp=vp->next) {
		flags = 0;
		if (extract_avp( vp, &flags, &name, &val)!=0 )
			continue;
		if (add_avp( flags, name, val) < 0) {
			LM_ERR("unable to create a new AVP\n");
		} else {
			LM_DBG("AVP '%.*s'/%d='%.*s'/%d has been added\n",
					(flags&AVP_NAME_STR)?name.s.len:4,
					(flags&AVP_NAME_STR)?name.s.s:"null",
					(flags&AVP_NAME_STR)?0:name.n,
					(flags&AVP_VAL_STR)?val.s.len:4,
					(flags&AVP_VAL_STR)?val.s.s:"null",
					(flags&AVP_VAL_STR)?0:val.n );
		}
	}

	return;
}

/* Macro to add extra attribute */
#define ADD_EXTRA_AVPAIR(_attrs, _attr, _val, _len)	\
	do { \
		if ((_len) != 0) { \
			if ((_len) == -1) { \
				if (_attrs[_attr].t != PW_TYPE_INTEGER) { \
					if (_attrs[_attr].t != PW_TYPE_IPADDR) { \
						LM_ERR("attribute %d is not of type integer or ipaddr\n", \
							_attrs[_attr].v); \
						goto error; \
					} \
				} \
			} \
			if (!rc_avpair_add( rh, &send, _attrs[_attr].v, _val, _len, 0)) { \
				LM_ERR("failed to add %s, %d\n", _attrs[_attr].n, _attr); \
				goto error; \
			} \
		} \
	}while(0)


/*
 * Loads from Radius caller's AVPs based on pvar argument.
 * Returns 1 if Radius request succeeded and -1 otherwise.
 */
int radius_load_caller_avps(struct sip_msg* _m, char* _caller, char* _s2)
{
	str user;
	VALUE_PAIR *send, *received;
	uint32_t service;
	static char msg[4096];
	int extra_cnt, offset, i, res;

	if ((_caller == NULL) ||
			(fixup_get_svalue(_m, (gparam_p)_caller, &user) != 0)) {
		LM_ERR("invalid caller parameter");
		return -1;
	}

	send = received = 0;

	if (!rc_avpair_add(rh, &send, caller_attrs[SA_USER_NAME].v,
				user.s, user.len, 0)) {
		LM_ERR("in adding SA_USER_NAME\n");
		return -1;
	}

	service = caller_vals[RV_SIP_CALLER_AVPS].v;
	if (!rc_avpair_add(rh, &send, caller_attrs[SA_SERVICE_TYPE].v,
				&service, -1, 0)) {
		LM_ERR("error adding SA_SERVICE_TYPE <%u>\n", service);
		goto error;
	}

	/* Add extra attributes */
	extra_cnt = extra2strar(caller_extra, _m, val_arr);
	if (extra_cnt == -1) {
		LM_ERR("in getting values of caller extra attributes\n");
		goto error;
	}
	offset = SA_STATIC_MAX;
	for (i = 0; i < extra_cnt; i++) {
		if (val_arr[i].len == -1) {
			/* Add integer attribute */
			ADD_EXTRA_AVPAIR(caller_attrs, offset+i,
					&(val_arr[i].s), val_arr[i].len );
		} else {
			/* Add string attribute */
			ADD_EXTRA_AVPAIR(caller_attrs, offset+i,
					val_arr[i].s, val_arr[i].len );
		}
	}

	if ((res = rc_auth(rh, 0, send, &received, msg)) == OK_RC) {
		LM_DBG("success\n");
		rc_avpair_free(send);
		if (common_response) {
			generate_avps_rad(received);
		} else {
			generate_avps(caller_attrs, received);
		}
		rc_avpair_free(received);
		return 1;
	} else {
		rc_avpair_free(send);
		if (common_response) generate_avps_rad(received);
		rc_avpair_free(received);
#ifdef REJECT_RC
		if (res == REJECT_RC) {
			LM_DBG("rejected\n");
			return -1;
		} else {
			LM_ERR("failure\n");
			return -2;
		}
#else
		LM_DBG("failure\n");
		return -1;
#endif
	}

error:
	rc_avpair_free(send);
	return -1;
}


/*
 * Loads from Radius callee's AVPs based on pvar argument.
 * Returns 1 if Radius request succeeded and -1 otherwise.
 */
int radius_load_callee_avps(struct sip_msg* _m, char* _callee, char* _s2)
{
	str user;
	VALUE_PAIR *send, *received;
	uint32_t service;
	static char msg[4096];
	int extra_cnt, offset, i, res;

	send = received = 0;

	if ((_callee == NULL) ||
			(fixup_get_svalue(_m, (gparam_p)_callee, &user) != 0)) {
		LM_ERR("invalid callee parameter");
		return -1;
	}

	if (!rc_avpair_add(rh, &send, callee_attrs[SA_USER_NAME].v,
				user.s, user.len, 0)) {
		LM_ERR("in adding SA_USER_NAME\n");
		return -1;
	}

	service = callee_vals[EV_SIP_CALLEE_AVPS].v;
	if (!rc_avpair_add(rh, &send, callee_attrs[SA_SERVICE_TYPE].v,
				&service, -1, 0)) {
		LM_ERR("in adding SA_SERVICE_TYPE <%u>\n", service);
		goto error;
	}

	/* Add extra attributes */
	extra_cnt = extra2strar(callee_extra, _m, val_arr);
	if (extra_cnt == -1) {
		LM_ERR("in getting values of callee extra attributes\n");
		goto error;
	}
	offset = SA_STATIC_MAX;
	for (i = 0; i < extra_cnt; i++) {
		if (val_arr[i].len == -1) {
			/* Add integer attribute */
			ADD_EXTRA_AVPAIR(callee_attrs, offset+i,
					&(val_arr[i].s), val_arr[i].len );
		} else {
			/* Add string attribute */
			ADD_EXTRA_AVPAIR(callee_attrs, offset+i,
					val_arr[i].s, val_arr[i].len );
		}
	}

	if ((res = rc_auth(rh, 0, send, &received, msg)) == OK_RC) {
		LM_DBG("success\n");
		rc_avpair_free(send);
		if (common_response) {
			generate_avps_rad(received);
		} else {
			generate_avps(callee_attrs, received);
		}
		rc_avpair_free(received);
		return 1;
	} else {
		rc_avpair_free(send);
		if (common_response) generate_avps_rad(received);
		rc_avpair_free(received);
#ifdef REJECT_RC
		if (res == REJECT_RC) {
			LM_DBG("rejected\n");
			return -1;
		} else {
			LM_ERR("failure\n");
			return -2;
		}
#else
		LM_DBG("failure\n");
		return -1;
#endif
	}

error:
	rc_avpair_free(send);
	return -1;
}


/*
 * Check from Radius if a user belongs to a group. User-Name is given in
 * first string argment that may contain pseudo variables.  SIP-Group is
 * given in second string variable that may not contain pseudo variables.
 * Service-Type is Group-Check.
 */
int radius_is_user_in(struct sip_msg* _m, char* _user, char* _group)
{
	str user, *group;
	VALUE_PAIR *send, *received;
	uint32_t service;
	static char msg[4096];
	int extra_cnt, offset, i, res;

	send = received = 0;

	if ((_user == NULL) ||
			(fixup_get_svalue(_m, (gparam_p)_user, &user) != 0)) {
		LM_ERR("invalid user parameter");
		return -1;
	}

	if (!rc_avpair_add(rh, &send, group_attrs[SA_USER_NAME].v,
				user.s, user.len, 0)) {
		LM_ERR("in adding SA_USER_NAME\n");
		return -1;
	}

	group = (str*)_group;
	if ((group == NULL) || (group->len == 0)) {
		LM_ERR("invalid group parameter");
		goto error;
	}
	if (!rc_avpair_add(rh, &send, group_attrs[SA_SIP_GROUP].v,
				group->s, group->len, 0)) {
		LM_ERR("in adding SA_SIP_GROUP\n");
		goto error;
	}

	service = group_vals[GV_GROUP_CHECK].v;
	if (!rc_avpair_add(rh, &send, group_attrs[SA_SERVICE_TYPE].v,
				&service, -1, 0)) {
		LM_ERR("in adding SA_SERVICE_TYPE <%u>\n", service);
		goto error;
	}

	/* Add extra attributes */
	extra_cnt = extra2strar(group_extra, _m, val_arr);
	if (extra_cnt == -1) {
		LM_ERR("in getting values of group extra attributes\n");
		goto error;
	}
	offset = SA_STATIC_MAX;
	for (i = 0; i < extra_cnt; i++) {
		if (val_arr[i].len == -1) {
			/* Add integer attribute */
			ADD_EXTRA_AVPAIR(group_attrs, offset+i,
					&(val_arr[i].s), val_arr[i].len );
		} else {
			/* Add string attribute */
			ADD_EXTRA_AVPAIR(group_attrs, offset+i,
					val_arr[i].s, val_arr[i].len );
		}
	}

	if ((res = rc_auth(rh, 0, send, &received, msg)) == OK_RC) {
		LM_DBG("success\n");
		rc_avpair_free(send);
		generate_avps(group_attrs, received);
		rc_avpair_free(received);
		return 1;
	} else {
		rc_avpair_free(send);
		rc_avpair_free(received);
#ifdef REJECT_RC
		if (res == REJECT_RC) {
			LM_DBG("rejected\n");
			return -1;
		} else {
			LM_ERR("failure\n");
			return -2;
		}
#else
		LM_DBG("failure\n");
		return -1;
#endif
	}

error:
	rc_avpair_free(send);
	return -1;
}

/*
 * Check from Radius if URI, whose user and host parts are given as
 * arguments, exists.  If so, loads AVPs based on reply items returned
 * from Radius.  If use_sip_uri_host module parameter has non-zero value,
 * user is send in SA_USER_NAME attribute and host in SA_SIP_URI_HOST
 * attribute.  If is has zero value, user@host is send in SA_USER_NAME
 * attribute.
 */
int radius_does_uri_user_host_exist(struct sip_msg* _m, str user, str host)
{
	char* at, *user_host;
	VALUE_PAIR *send, *received;
	uint32_t service;
	static char msg[4096];
	int extra_cnt, offset, i, res;

	send = received = 0;
	user_host = 0;

	if (!use_sip_uri_host) {

		/* Send user@host in SA_USER_NAME attr */
		user_host = (char*)pkg_malloc(user.len + host.len + 2);
		if (!user_host) {
			LM_ERR("no more pkg memory\n");
			return -1;
		}
		at = user_host;
		memcpy(at, user.s, user.len);
		at += user.len;
		*at = '@';
		at++;
		memcpy(at , host.s, host.len);
		at += host.len;
		*at = '\0';
		if (!rc_avpair_add(rh, &send, uri_attrs[SA_USER_NAME].v, user_host,
					-1, 0)) {
			LM_ERR("in adding SA_USER_NAME\n");
			pkg_free(user_host);
			return -1;
		}

	} else {

		/* Send user in SA_USER_NAME attribute and host in SA_SIP_URI_HOST
		   attribute */
		if (!rc_avpair_add(rh, &send, uri_attrs[SA_USER_NAME].v,
					user.s, user.len, 0)) {
			LM_ERR("adding User-Name failed\n");
			return -1;
		}
		if (!rc_avpair_add(rh, &send, uri_attrs[SA_SIP_URI_HOST].v,
					host.s, host.len, 0)) {
			LM_ERR("adding SIP-URI-Host failed\n");
			goto error;
		}
	}

	service = uri_vals[UV_CALL_CHECK].v;
	if (!rc_avpair_add(rh, &send, uri_attrs[SA_SERVICE_TYPE].v,
				&service, -1, 0)) {
		LM_ERR("in adding SA_SERVICE_TYPE <%u>\n", service);
		goto error;
	}

	/* Add extra attributes */
	extra_cnt = extra2strar(uri_extra, _m, val_arr);
	if (extra_cnt == -1) {
		LM_ERR("in getting values of group extra attributes\n");
		goto error;
	}
	offset = SA_STATIC_MAX;
	for (i = 0; i < extra_cnt; i++) {
		if (val_arr[i].len == -1) {
			/* Add integer attribute */
			ADD_EXTRA_AVPAIR(uri_attrs, offset+i,
					&(val_arr[i].s), val_arr[i].len );
		} else {
			/* Add string attribute */
			ADD_EXTRA_AVPAIR(uri_attrs, offset+i,
					val_arr[i].s, val_arr[i].len );
		}
	}

	if ((res = rc_auth(rh, 0, send, &received, msg)) == OK_RC) {
		LM_DBG("success\n");
		if (user_host) pkg_free(user_host);
		rc_avpair_free(send);
		generate_avps(uri_attrs, received);
		rc_avpair_free(received);
		return 1;
	} else {
		if (user_host) pkg_free(user_host);
		rc_avpair_free(send);
		rc_avpair_free(received);
#ifdef REJECT_RC
		if (res == REJECT_RC) {
			LM_DBG("rejected\n");
			return -1;
		} else {
			LM_ERR("failure\n");
			return -2;
		}
#else
		LM_DBG("failure\n");
		return -1;
#endif
	}

error:
	rc_avpair_free(send);
	if (user_host) pkg_free(user_host);
	return -1;
}


/*
 * Check from Radius if Request URI belongs to a local user.
 * If so, loads AVPs based on reply items returned from Radius.
 */
int radius_does_uri_exist_0(struct sip_msg* _m, char* _s1, char* _s2)
{

	if (parse_sip_msg_uri(_m) < 0) {
		LM_ERR("parsing Request-URI failed\n");
		return -1;
	}

	return radius_does_uri_user_host_exist(_m, _m->parsed_uri.user,
			_m->parsed_uri.host);
}


/*
 * Check from Radius if URI given in pvar argument belongs to a local user.
 * If so, loads AVPs based on reply items returned from Radius.
 */
int radius_does_uri_exist_1(struct sip_msg* _m, char* _sp, char* _s2)
{
	pv_spec_t *sp;
	pv_value_t pv_val;
	struct sip_uri parsed_uri;

	sp = (pv_spec_t *)_sp;

	if (sp && (pv_get_spec_value(_m, sp, &pv_val) == 0)) {
		if (pv_val.flags & PV_VAL_STR) {
			if (pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
				LM_ERR("pvar argument is empty\n");
				return -1;
			}
		} else {
			LM_ERR("pvar value is not string\n");
			return -1;
		}
	} else {
		LM_ERR("cannot get pvar value\n");
		return -1;
	}

	if (parse_uri(pv_val.rs.s, pv_val.rs.len, &parsed_uri) < 0) {
		LM_ERR("parsing of URI in pvar failed\n");
		return -1;
	}

	return radius_does_uri_user_host_exist(_m, parsed_uri.user,
			parsed_uri.host);
}


/*
 * Check from Radius if URI user given as argument belongs to a local user.
 * If so, loads AVPs based on reply items returned from Radius.
 */
int radius_does_uri_user_exist(struct sip_msg* _m, str user)
{
	static char msg[4096];
	VALUE_PAIR *send, *received;
	uint32_t service;
	int res, extra_cnt, offset, i;

	send = received = 0;

	if (!rc_avpair_add(rh, &send, uri_attrs[SA_USER_NAME].v,
				user.s, user.len, 0)) {
		LM_ERR("in adding SA_USER_NAME\n");
		return -1;
	}

	service = uri_vals[UV_CALL_CHECK].v;
	if (!rc_avpair_add(rh, &send, uri_attrs[SA_SERVICE_TYPE].v,
				&service, -1, 0)) {
		LM_ERR("in adding SA_SERVICE_TYPE <%u>\n", service);
		goto error;
	}

	/* Add extra attributes */
	extra_cnt = extra2strar(uri_extra, _m, val_arr);
	if (extra_cnt == -1) {
		LM_ERR("in getting values of group extra attributes\n");
		goto error;
	}
	offset = SA_STATIC_MAX;
	for (i = 0; i < extra_cnt; i++) {
		if (val_arr[i].len == -1) {
			/* Add integer attribute */
			ADD_EXTRA_AVPAIR(uri_attrs, offset+i,
					&(val_arr[i].s), val_arr[i].len );
		} else {
			/* Add string attribute */
			ADD_EXTRA_AVPAIR(uri_attrs, offset+i,
					val_arr[i].s, val_arr[i].len );
		}
	}

	if ((res = rc_auth(rh, 0, send, &received, msg)) == OK_RC) {
		LM_DBG("success\n");
		rc_avpair_free(send);
		generate_avps(uri_attrs, received);
		rc_avpair_free(received);
		return 1;
	} else {
		rc_avpair_free(send);
		rc_avpair_free(received);
#ifdef REJECT_RC
		if (res == REJECT_RC) {
			LM_DBG("rejected\n");
			return -1;
		} else {
			LM_ERR("failure\n");
			return -2;
		}
#else
		LM_DBG("failure\n");
		return -1;
#endif
	}

error:
	rc_avpair_free(send);
	return -1;
}


/*
 * Check from Radius if Request URI user belongs to a local user.
 * If so, loads AVPs based on reply items returned from Radius.
 */
int radius_does_uri_user_exist_0(struct sip_msg* _m, char* _s1, char* _s2)
{

	if (parse_sip_msg_uri(_m) < 0) {
		LM_ERR("parsing Request-URI failed\n");
		return -1;
	}

	return radius_does_uri_user_exist(_m, _m->parsed_uri.user);
}


/*
 * Check from Radius if URI user given in pvar argument belongs
 * to a local user. If so, loads AVPs based on reply items returned
 * from Radius. 
 */
int radius_does_uri_user_exist_1(struct sip_msg* _m, char* _sp, char* _s2)
{
	pv_spec_t *sp;
	pv_value_t pv_val;

	sp = (pv_spec_t *)_sp;

	if (sp && (pv_get_spec_value(_m, sp, &pv_val) == 0)) {
		if (pv_val.flags & PV_VAL_STR) {
			if (pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
				LM_ERR("pvar argument is empty\n");
				return -1;
			}
		} else {
			LM_ERR("pvar value is not string\n");
			return -1;
		}
	} else {
		LM_ERR("cannot get pvar value\n");
		return -1;
	}

	return radius_does_uri_user_exist(_m, pv_val.rs);
}
