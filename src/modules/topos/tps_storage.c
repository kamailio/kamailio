/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief Kamailio topos ::
 * \ingroup topos
 * Module: \ref topos
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../core/dprint.h"
#include "../../core/hashes.h"
#include "../../core/locking.h"
#include "../../core/trim.h"
#include "../../core/ut.h"
#include "../../core/xavp.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/contact/contact.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_expires.h"

#include "../../lib/srdb1/db.h"
#include "../../core/utils/sruid.h"

#include "tps_storage.h"
#include "api.h"

extern sruid_t _tps_sruid;

extern db1_con_t *_tps_db_handle;
extern db_func_t _tpsdbf;

extern str _tps_contact_host;
extern int _tps_contact_mode;
extern str _tps_cparam_name;
extern str _tps_xavu_cfg;
extern str _tps_xavu_field_acontact;
extern str _tps_xavu_field_bcontact;
extern str _tps_xavu_field_contact_host;
extern str _tps_xavu_field_acontact_host;
extern str _tps_xavu_field_bcontact_host;
extern int _tps_methods_update_time;
extern int _tps_reg_pub_multi_contact;

extern str _tps_context_param;
extern str _tps_context_value;

#define TPS_STORAGE_LOCK_SIZE 1 << 9
static gen_lock_set_t *_tps_storage_lock_set = NULL;

int _tps_branch_expire = 180;
int _tps_dialog_expire = 10800;

int tps_db_insert_dialog(tps_data_t *td);
int tps_db_clean_dialogs(void);
int tps_db_insert_branch(tps_data_t *td);
int tps_db_clean_branches(void);
int tps_db_load_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode);
int tps_db_load_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);
int tps_db_load_dialog_by_tags(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);
int tps_db_update_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode);
int tps_db_update_dialog(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode);
int tps_db_end_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);

static void init_contact_data(
		tps_data_t *td, str *uuid, str *contact_sv, sip_uri_t *puri, int dir);
static void append_host_info(tps_data_t *td, sip_uri_t *puri, sr_xavp_t *vavu,
		int append_port_proto);
static void add_uuid_param(tps_data_t *td, str *uuid, int dir);
static sr_xavp_t *get_xavu_host(int dir);

/**
 *
 */
static tps_storage_api_t _tps_storage_api = {
		.insert_dialog = tps_db_insert_dialog,
		.clean_dialogs = tps_db_clean_dialogs,
		.insert_branch = tps_db_insert_branch,
		.clean_branches = tps_db_clean_branches,
		.load_branch = tps_db_load_branch,
		.load_dialog = tps_db_load_dialog,
		.load_dialog_by_tags = tps_db_load_dialog_by_tags,
		.update_branch = tps_db_update_branch,
		.update_dialog = tps_db_update_dialog,
		.end_dialog = tps_db_end_dialog};

/**
 *
 */
int tps_set_storage_api(tps_storage_api_t *tsa)
{
	if(tsa == NULL)
		return -1;
	LM_DBG("setting new storage api: %p\n", tsa);
	memcpy(&_tps_storage_api, tsa, sizeof(tps_storage_api_t));

	return 0;
}

/**
 *
 */
int tps_storage_lock_set_init(void)
{
	_tps_storage_lock_set = lock_set_alloc(TPS_STORAGE_LOCK_SIZE);
	if(_tps_storage_lock_set == NULL
			|| lock_set_init(_tps_storage_lock_set) == NULL) {
		LM_ERR("cannot initiate lock set\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int tps_storage_lock_get(str *lkey)
{
	uint32_t pos;
	pos = core_case_hash(lkey, 0, TPS_STORAGE_LOCK_SIZE);
	LM_DBG("tps lock get: %u\n", pos);
	lock_set_get(_tps_storage_lock_set, pos);
	return 1;
}

/**
 *
 */
int tps_storage_lock_release(str *lkey)
{
	uint32_t pos;
	pos = core_case_hash(lkey, 0, TPS_STORAGE_LOCK_SIZE);
	LM_DBG("tps lock release: %u\n", pos);
	lock_set_release(_tps_storage_lock_set, pos);
	return 1;
}

/**
 *
 */
int tps_storage_lock_set_destroy(void)
{
	if(_tps_storage_lock_set != NULL) {
		lock_set_destroy(_tps_storage_lock_set);
		lock_set_dealloc(_tps_storage_lock_set);
	}
	_tps_storage_lock_set = NULL;
	return 0;
}

/**
 *
 */
int tps_storage_dialog_find(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}

/**
 *
 */
int tps_storage_dialog_save(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}

/**
 *
 */
int tps_storage_dialog_rm(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}


/**
 *
 */
int tps_storage_branch_find(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}

/**
 *
 */
int tps_storage_branch_save(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}

/**
 *
 */
int tps_storage_branch_rm(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}

/**
 *
 */
static size_t tps_contact_host_len(sip_uri_t *puri, sr_xavp_t *vavu)
{
	if((vavu == NULL || vavu->val.v.s.len <= 0) && _tps_xavu_cfg.len > 0
			&& _tps_xavu_field_contact_host.len > 0) {
		vavu = xavu_get_child_with_sval(
				&_tps_xavu_cfg, &_tps_xavu_field_contact_host);
	}

	if(vavu != NULL && vavu->val.v.s.len > 0) {
		return (size_t)vavu->val.v.s.len;
	}
	if(_tps_contact_host.len > 0) {
		return (size_t)_tps_contact_host.len;
	}
	return (size_t)puri->host.len;
}

/**
 *
 */
int tps_storage_fill_contact(
		sip_msg_t *msg, tps_data_t *td, str *uuid, int dir, int ctmode)
{
	str sv;
	str cuser = STR_NULL;
	sip_uri_t puri;
	sip_uri_t curi;
	contact_body_t *cb;
	size_t needed;
	size_t host_len;
	size_t scheme_len;
	int append_port_proto;
	int metid;
	sr_xavp_t *vavu = NULL;
	sr_xavp_t *uavu = NULL;
	str *field;

	if(dir == TPS_DIR_DOWNSTREAM) {
		sv = td->bs_contact;
	} else {
		sv = td->as_contact;
	}
	if(sv.len <= 0) {
		/* no contact - skip */
		return 0;
	}

	if(parse_uri(sv.s, sv.len, &puri) < 0) {
		LM_ERR("failed to parse the uri\n");
		return -1;
	}

	scheme_len = 0;
	while(scheme_len < (size_t)sv.len) {
		scheme_len++;
		if(sv.s[scheme_len - 1] == ':') {
			break;
		}
	}

	append_port_proto = (ctmode == TPS_CONTACT_MODE_SKEYUSER
								|| ctmode == TPS_CONTACT_MODE_RURIUSER
								|| ctmode == TPS_CONTACT_MODE_XAVPUSER)
								? 1
								: 0;

	if(ctmode == TPS_CONTACT_MODE_RURIUSER) {
		if(dir == TPS_DIR_DOWNSTREAM) {
			if(parse_headers(msg, HDR_CSEQ_F, 0) != 0 || msg->cseq == NULL
					|| get_cseq(msg) == NULL) {
				LM_ERR("bad sip message or missing CSeq header\n");
				return -1;
			}
			metid = get_cseq(msg)->method_id;
			if(parse_headers(msg, HDR_CONTACT_F, 0) < 0
					|| msg->contact == NULL) {
				if(metid != METHOD_BYE) {
					LM_WARN("bad sip message or missing Contact header\n");
					return -1;
				}
				LM_DBG("BYE with no contact - skipping it\n");
				return 0;
			}

			if(parse_contact(msg->contact) < 0
					|| ((contact_body_t *)msg->contact->parsed)->contacts
							   == NULL
					|| ((contact_body_t *)msg->contact->parsed)->contacts->next
							   != NULL) {
				LM_ERR("bad Contact header\n");
				return -1;
			}

			cb = (contact_body_t *)msg->contact->parsed;
			if(parse_uri(cb->contacts->uri.s, cb->contacts->uri.len, &curi)
					< 0) {
				LM_ERR("failed to parse the contact uri\n");
				return -1;
			}

			if(curi.user.len > 0) {
				cuser = curi.user;
			}
		} else {
			if(parse_sip_msg_uri(msg) < 0) {
				LM_ERR("failed to parse r-uri\n");
				return -1;
			}
			if(msg->parsed_uri.user.len > 0) {
				cuser = msg->parsed_uri.user;
			}
		}
	} else if(ctmode == TPS_CONTACT_MODE_XAVPUSER) {
		if(dir == TPS_DIR_DOWNSTREAM) {
			field = &_tps_xavu_field_acontact;
		} else {
			field = &_tps_xavu_field_bcontact;
		}
		uavu = xavu_get_child_with_sval(&_tps_xavu_cfg, field);
		if(uavu == NULL || uavu->val.v.s.len <= 0) {
			LM_ERR("could not evaluate %s contact xavu\n",
					(dir == TPS_DIR_DOWNSTREAM) ? "a" : "b");
			return -1;
		}
		cuser = uavu->val.v.s;
	} else if(ctmode == TPS_CONTACT_MODE_XAVPHOST) {
		/* extract the contact host from xavp */
		vavu = get_xavu_host(dir);
	}

	host_len = tps_contact_host_len(&puri, vavu);
	needed = 0;
	needed += 1 + (size_t)uuid->len; /* saved a/b uuid */
	needed += 1 + scheme_len;		 /* '<' + scheme */

	if(ctmode == TPS_CONTACT_MODE_RURIUSER
			|| ctmode == TPS_CONTACT_MODE_XAVPUSER) {
		needed += (size_t)cuser.len;
		if(cuser.len > 0) {
			needed += 1; /* '@' */
		}
		needed += host_len;
		if(append_port_proto && puri.port.len > 0) {
			needed += 1 + (size_t)puri.port.len;
		}
		if(append_port_proto && puri.transport_val.len > 0) {
			needed += 11 + (size_t)puri.transport_val.len;
		}
		needed += 1 + (size_t)_tps_cparam_name.len + 1 + 1 + (size_t)uuid->len;
	} else {
		needed += 1 + (size_t)uuid->len + 1; /* a|b + uuid + '@' */
		needed += host_len;
		if(append_port_proto && puri.port.len > 0) {
			needed += 1 + (size_t)puri.port.len;
		}
		if(append_port_proto && puri.transport_val.len > 0) {
			needed += 11 + (size_t)puri.transport_val.len;
		}
	}
	needed += 1; /* '>' */

	if(td->cp + needed > td->cbuf + TPS_DATA_SIZE) {
		LM_ERR("insufficient data buffer\n");
		return -1;
	}

	/* Initialize contact data */
	init_contact_data(td, uuid, &sv, &puri, dir);

	/* Process contact based on mode */
	if(ctmode == TPS_CONTACT_MODE_RURIUSER
			|| ctmode == TPS_CONTACT_MODE_XAVPUSER) {
		/* create new URI parameter for Contact header */
		if(cuser.len > 0) {
			memcpy(td->cp, cuser.s, cuser.len);
			td->cp += cuser.len;
		}

		if(cuser.len > 0) {
			*td->cp = '@';
			td->cp++;
		}

		append_host_info(td, &puri, NULL, 1);
		add_uuid_param(td, uuid, dir);

	} else {
		/* contact_mode=0 or contact_mode=3 */
		/* create new user part for Contact header URI */
		if(dir == TPS_DIR_DOWNSTREAM) {
			*td->cp = 'b';
		} else {
			*td->cp = 'a';
		}
		td->cp++;
		memcpy(td->cp, uuid->s, uuid->len);
		td->cp += uuid->len;
		*td->cp = '@';
		td->cp++;

		/* ctmode=0 preserves port/transport from original URI; ctmode=3 does not */
		append_host_info(
				td, &puri, vavu, (ctmode == TPS_CONTACT_MODE_SKEYUSER) ? 1 : 0);
	}

	/* Finalize the contact header */
	*td->cp = '>';
	td->cp++;
	if(dir == TPS_DIR_DOWNSTREAM) {
		td->bs_contact.len = td->cp - td->bs_contact.s;
		LM_DBG("td->bs %.*s\n", td->bs_contact.len, td->bs_contact.s);
	} else {
		td->as_contact.len = td->cp - td->as_contact.s;
		LM_DBG("td->as %.*s\n", td->as_contact.len, td->as_contact.s);
	}
	return 0;
}

/* Get the contact host xavu from the xavu defined in the config

	dir: TPS_DIR_DOWNSTREAM or TPS_DIR_UPSTREAM
	return: the xavu or NULL if not found

	TODO: Who is responsible to free the xavu?
*/
static sr_xavp_t *get_xavu_host(int dir)
{
	/* Downstream should be B side, upstream should be A side
	otherwise the config related params are used in reversed for some reason */
	sr_xavp_t *vavu;
	str *field;

	if(dir == TPS_DIR_DOWNSTREAM) {
		field = &_tps_xavu_field_bcontact_host;
	} else {
		field = &_tps_xavu_field_acontact_host;
	}

	vavu = xavu_get_child_with_sval(&_tps_xavu_cfg, field);
	if(vavu == NULL || vavu->val.v.s.len <= 0) {
		LM_ERR("could not evaluate %s_contact_host xavu. Make sure it has a "
			   "non-empty value or it is set in the related routes.\n",
				(dir == TPS_DIR_DOWNSTREAM) ? "b" : "a");
		return NULL;
	}
	return vavu;
}

/* Initialize the contact data for the given direction and save uuid for later use*/
static void init_contact_data(
		tps_data_t *td, str *uuid, str *contact_sv, sip_uri_t *puri, int dir)
{
	/* Copy uuid based on direction */
	if(dir == TPS_DIR_DOWNSTREAM) {
		td->b_uuid.s = td->cp;
		*td->cp = 'b';
		td->cp++;
		memcpy(td->cp, uuid->s, uuid->len);
		td->cp += uuid->len;
		td->b_uuid.len = td->cp - td->b_uuid.s;

		td->bs_contact.s = td->cp;
	} else {
		td->a_uuid.s = td->cp;
		*td->cp = 'a';
		td->cp++;
		memcpy(td->cp, uuid->s, uuid->len);
		td->cp += uuid->len;
		td->a_uuid.len = td->cp - td->a_uuid.s;

		td->as_contact.s = td->cp;
	}

	/* Initialize contact */
	*td->cp = '<';
	td->cp++;

	/* Copy up until SIP scheme */
	int i;
	for(i = 0; i < contact_sv->len; i++) {
		*td->cp = contact_sv->s[i];
		td->cp++;
		if(contact_sv->s[i] == ':') {
			break;
		}
	}
}

/* Append host info to contact
	* @in/out td - tps data
	* @in puri - sip uri
	* @in vavu - xavu_field_a/b_contact_host or Null

	Modifies the contact in td to include the host info with priorities:
	1. xavu_field_a/b_contact_host if provided
	2. xavu field xavu_field_contact_host
	3. mod param contact_host
	4. sip uri host

	Also include port and protocol if present
 */
static void append_host_info(
		tps_data_t *td, sip_uri_t *puri, sr_xavp_t *vavu, int append_port_proto)
{
	/* contact_host xavu takes preference */
	if(vavu == NULL || vavu->val.v.s.len <= 0) {
		if(_tps_xavu_cfg.len > 0 && _tps_xavu_field_contact_host.len > 0) {
			vavu = xavu_get_child_with_sval(
					&_tps_xavu_cfg, &_tps_xavu_field_contact_host);
		}
	}

	if(vavu != NULL && vavu->val.v.s.len > 0) {
		memcpy(td->cp, vavu->val.v.s.s, vavu->val.v.s.len);
		td->cp += vavu->val.v.s.len;
	} else {
		if(_tps_contact_host.len) {
			memcpy(td->cp, _tps_contact_host.s, _tps_contact_host.len);
			td->cp += _tps_contact_host.len;
		} else {
			memcpy(td->cp, puri->host.s, puri->host.len);
			td->cp += puri->host.len;
		}
	}

	/*  Add port if present  */
	if(append_port_proto) {
		if(puri->port.len > 0) {
			*td->cp = ':';
			td->cp++;
			memcpy(td->cp, puri->port.s, puri->port.len);
			td->cp += puri->port.len;
		}

		/*  Add transport if present */
		if(puri->transport_val.len > 0) {
			memcpy(td->cp, ";transport=", 11);
			td->cp += 11;
			memcpy(td->cp, puri->transport_val.s, puri->transport_val.len);
			td->cp += puri->transport_val.len;
		}
	}
}

/* Add contact parameter name and uuid to the contact parameters */
static void add_uuid_param(tps_data_t *td, str *uuid, int dir)
{
	*td->cp = ';';
	td->cp++;
	memcpy(td->cp, _tps_cparam_name.s, _tps_cparam_name.len);
	td->cp += _tps_cparam_name.len;
	*td->cp = '=';
	td->cp++;
	*td->cp = (dir == TPS_DIR_DOWNSTREAM) ? 'b' : 'a';
	td->cp++;
	memcpy(td->cp, uuid->s, uuid->len);
	td->cp += uuid->len;
}


/**
 *
 */
int tps_storage_link_msg(sip_msg_t *msg, tps_data_t *td, int dir)
{
	str stxt;

	if(parse_headers(msg, HDR_EOH_F, 0) == -1)
		return -1;

	/* callid */
	stxt = msg->callid->body;
	trim(&stxt);
	td->a_callid = stxt;

	/* get from-tag */
	if(parse_from_header(msg) < 0 || msg->from == NULL) {
		LM_ERR("failed getting 'from' header!\n");
		goto error;
	}
	td->a_tag = get_from(msg)->tag_value;

	/* get to-tag */
	if(parse_to_header(msg) < 0 || msg->to == NULL) {
		LM_ERR("failed getting 'to' header!\n");
		goto error;
	}
	td->b_tag = get_to(msg)->tag_value;

	if(dir == TPS_DIR_DOWNSTREAM) {
		td->x_tag = td->a_tag;
	} else {
		td->x_tag = td->b_tag;
	}

	td->x_via = td->x_via2;
	if(parse_headers(msg, HDR_CSEQ_F, 0) != 0 || msg->cseq == NULL) {
		LM_ERR("cannot parse cseq header\n");
		return -1; /* should it be 0 ?!?! */
	}
	td->s_method = get_cseq(msg)->method;
	td->s_cseq = get_cseq(msg)->number;
	td->s_method_id = get_cseq(msg)->method_id;

	/* extract the contact address */
	if(parse_headers(msg, HDR_CONTACT_F, 0) < 0 || msg->contact == NULL) {
		if((td->s_method_id != METHOD_INVITE)
				&& (td->s_method_id != METHOD_SUBSCRIBE)
				&& (td->s_method_id != METHOD_REGISTER)
				&& (td->s_method_id != METHOD_PUBLISH)) {
			/* no mandatory contact unless dialog-creating / reg / publish - done */
			return 0;
		}
		if(msg->first_line.type == SIP_REPLY) {
			if(msg->first_line.u.reply.statuscode >= 100
					&& msg->first_line.u.reply.statuscode < 200) {
				/* provisional response with no mandatory contact header */
				return 0;
			}
			if(msg->first_line.u.reply.statuscode >= 400) {
				/* failure response with no mandatory contact header */
				return 0;
			}
		}
		LM_ERR("bad sip message or missing Contact hdr\n");
		goto error;
	}
	if(parse_contact(msg->contact) < 0
			|| ((contact_body_t *)msg->contact->parsed)->contacts == NULL) {
		LM_ERR("bad Contact header\n");
		return -1;
	}
	if(((contact_body_t *)msg->contact->parsed)->contacts->next != NULL) {
		if((td->s_method_id == METHOD_REGISTER)
				|| (td->s_method_id == METHOD_PUBLISH)) {
			if(_tps_reg_pub_multi_contact == 1) {
				LM_ERR("topos: multi-Contact body rejected "
					   "(reg_pub_multi_contact=1), method %u\n",
						td->s_method_id);
				return -1;
			}
			LM_NOTICE("topos: multi-Contact body — using first URI only for "
					  "topology storage (method %u); full bindings belong to "
					  "registrar/usrloc (e.g. ims_registrar_pcscf / "
					  "ims_usrloc_pcscf)\n",
					td->s_method_id);
		} else {
			LM_ERR("bad Contact header (multiple contacts)\n");
			return -1;
		}
	}

	if(msg->first_line.type == SIP_REQUEST) {
		if(dir == TPS_DIR_DOWNSTREAM) {
			td->a_contact =
					((contact_body_t *)msg->contact->parsed)->contacts->uri;
		} else {
			td->b_contact =
					((contact_body_t *)msg->contact->parsed)->contacts->uri;
		}
	} else {
		if(dir == TPS_DIR_DOWNSTREAM) {
			td->b_contact =
					((contact_body_t *)msg->contact->parsed)->contacts->uri;
		} else {
			td->a_contact =
					((contact_body_t *)msg->contact->parsed)->contacts->uri;
		}
	}

	if(td->s_method_id == METHOD_SUBSCRIBE || td->s_method_id == METHOD_PUBLISH
			|| td->s_method_id == METHOD_REGISTER) {
		contact_t *ct = NULL;
		if(msg->contact != NULL && msg->contact->parsed != NULL) {
			ct = ((contact_body_t *)msg->contact->parsed)->contacts;
		}
		tps_data_fill_expires(msg, td, ct);
	}


	LM_DBG("downstream: %s - acontact: [%.*s] - bcontact: [%.*s]\n",
			(dir == TPS_DIR_DOWNSTREAM) ? "yes" : "no", td->a_contact.len,
			(td->a_contact.len > 0) ? td->a_contact.s : "", td->b_contact.len,
			(td->b_contact.len > 0) ? td->b_contact.s : "");

	return 0;

error:
	return -1;
}

/**
 *
 */
int tps_storage_record(sip_msg_t *msg, tps_data_t *td, int dialog, int dir)
{
	int ret = -1; /* error if dialog == 0 */
	int metid;
	str suid;
	str *sx = NULL;

	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		return -1;
	}
	if(parse_headers(msg, HDR_CSEQ_F, 0) != 0 || msg->cseq == NULL
			|| get_cseq(msg) == NULL) {
		LM_ERR("bad sip message or missing CSeq header\n");
		return -1;
	}
	metid = get_cseq(msg)->method_id;

	if(metid == METHOD_ACK) {
		if(parse_headers(msg, HDR_CONTACT_F, 0) < 0 || msg->contact == NULL) {
			/* ACK with no Contact - nothing to store */
			return 0;
		}
	}
	if(_tps_context_value.len > 0) {
		sx = &_tps_context_value;
	} else if(_tps_context_param.len > 0) {
		sx = &_tps_context_param;
	}

	if(dialog == 0) {
		sruid_nextx(&_tps_sruid, sx);
		suid = _tps_sruid.uid;
	} else {
		if(td->a_uuid.len > 0) {
			suid = td->a_uuid;
		} else if(td->b_uuid.len > 0) {
			suid = td->b_uuid;
		} else {
			goto error;
		}
		suid.s++;
		suid.len--;
	}

	ret = tps_storage_fill_contact(
			msg, td, &suid, TPS_DIR_DOWNSTREAM, _tps_contact_mode);
	if(ret < 0)
		goto error;
	ret = tps_storage_fill_contact(
			msg, td, &suid, TPS_DIR_UPSTREAM, _tps_contact_mode);
	if(ret < 0)
		goto error;

	ret = tps_storage_link_msg(msg, td, dir);
	if(ret < 0)
		goto error;
	ret = _tps_storage_api.insert_branch(td);
	if(ret < 0)
		goto error;
	if(dialog == 0) {
		if(td->as_contact.len <= 0 && td->bs_contact.len <= 0) {
			LM_WARN("no local address - do record routing for all initial "
					"requests\n");
		}
		ret = _tps_storage_api.insert_dialog(td);
		if(ret < 0)
			goto error;
	}

	return 0;

error:
	LM_ERR("failed to store (dlg: %d dir: %d metid: %d)\n", dialog, dir,
			get_cseq(msg)->method_id);
	return ret;
}


/**
 * database storage
 */
str td_table_name = str_init("topos_d");
str td_col_rectime = str_init("rectime");
str td_col_a_callid = str_init("a_callid");
str td_col_a_uuid = str_init("a_uuid");
str td_col_b_uuid = str_init("b_uuid");
str td_col_a_contact = str_init("a_contact");
str td_col_b_contact = str_init("b_contact");
str td_col_as_contact = str_init("as_contact");
str td_col_bs_contact = str_init("bs_contact");
str td_col_a_tag = str_init("a_tag");
str td_col_b_tag = str_init("b_tag");
str td_col_a_rr = str_init("a_rr");
str td_col_b_rr = str_init("b_rr");
str td_col_s_rr = str_init("s_rr");
str td_col_iflags = str_init("iflags");
str td_col_a_uri = str_init("a_uri");
str td_col_b_uri = str_init("b_uri");
str td_col_r_uri = str_init("r_uri");
str td_col_a_srcaddr = str_init("a_srcaddr");
str td_col_b_srcaddr = str_init("b_srcaddr");
str td_col_s_method = str_init("s_method");
str td_col_s_cseq = str_init("s_cseq");
str td_col_x_context = str_init("x_context");

str tt_table_name = str_init("topos_t");
str tt_col_rectime = str_init("rectime");
str tt_col_a_callid = str_init("a_callid");
str tt_col_a_uuid = str_init("a_uuid");
str tt_col_b_uuid = str_init("b_uuid");
str tt_col_direction = str_init("direction");
str tt_col_x_via = str_init("x_via");
str tt_col_x_vbranch = str_init("x_vbranch");
str tt_col_x_rr = str_init("x_rr");
str tt_col_y_rr = str_init("y_rr");
str tt_col_s_rr = str_init("s_rr");
str tt_col_x_uri = str_init("x_uri");
str tt_col_a_contact = str_init("a_contact");
str tt_col_b_contact = str_init("b_contact");
str tt_col_as_contact = str_init("as_contact");
str tt_col_bs_contact = str_init("bs_contact");
str tt_col_x_tag = str_init("x_tag");
str tt_col_a_tag = str_init("a_tag");
str tt_col_b_tag = str_init("b_tag");
str tt_col_s_method = str_init("s_method");
str tt_col_s_cseq = str_init("s_cseq");
str tt_col_x_context = str_init("x_context");

#define TPS_NR_KEYS 48

str _tps_empty = str_init("");

#define TPS_STRZ(_s) ((_s).s) ? (_s) : (_tps_empty)
/**
 *
 */
int tps_db_insert_dialog(tps_data_t *td)
{
	db_key_t db_keys[TPS_NR_KEYS];
	db_val_t db_vals[TPS_NR_KEYS];
	int nr_keys;

	if(_tps_db_handle == NULL) {
		LM_ERR("No database handle - misconfiguration?\n");
		goto error;
	}

	memset(db_keys, 0, TPS_NR_KEYS * sizeof(db_key_t));
	memset(db_vals, 0, TPS_NR_KEYS * sizeof(db_val_t));
	nr_keys = 0;

	db_keys[nr_keys] = &td_col_rectime;
	db_vals[nr_keys].type = DB1_DATETIME;
	db_vals[nr_keys].val.time_val = time(NULL);
	nr_keys++;

	db_keys[nr_keys] = &td_col_a_callid;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_callid);
	nr_keys++;

	db_keys[nr_keys] = &td_col_a_uuid;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_uuid);
	nr_keys++;

	db_keys[nr_keys] = &td_col_b_uuid;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->b_uuid);
	nr_keys++;

	db_keys[nr_keys] = &td_col_a_contact;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_contact);
	nr_keys++;

	db_keys[nr_keys] = &td_col_b_contact;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->b_contact);
	nr_keys++;

	db_keys[nr_keys] = &td_col_as_contact;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->as_contact);
	nr_keys++;

	db_keys[nr_keys] = &td_col_bs_contact;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->bs_contact);
	nr_keys++;

	db_keys[nr_keys] = &td_col_a_tag;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_tag);
	nr_keys++;

	db_keys[nr_keys] = &td_col_b_tag;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->b_tag);
	nr_keys++;

	db_keys[nr_keys] = &td_col_a_rr;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_rr);
	nr_keys++;

	db_keys[nr_keys] = &td_col_b_rr;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->b_rr);
	nr_keys++;

	db_keys[nr_keys] = &td_col_s_rr;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->s_rr);
	nr_keys++;

	db_keys[nr_keys] = &td_col_iflags;
	db_vals[nr_keys].type = DB1_INT;
	db_vals[nr_keys].val.int_val = td->iflags;
	nr_keys++;

	db_keys[nr_keys] = &td_col_a_uri;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_uri);
	nr_keys++;

	db_keys[nr_keys] = &td_col_b_uri;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->b_uri);
	nr_keys++;

	db_keys[nr_keys] = &td_col_r_uri;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->r_uri);
	nr_keys++;

	db_keys[nr_keys] = &td_col_a_srcaddr;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_srcaddr);
	nr_keys++;

	db_keys[nr_keys] = &td_col_b_srcaddr;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->b_srcaddr);
	nr_keys++;

	db_keys[nr_keys] = &td_col_s_method;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->s_method);
	nr_keys++;

	db_keys[nr_keys] = &td_col_s_cseq;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->s_cseq);
	nr_keys++;

	if(td->x_context.len > 0) {
		db_keys[nr_keys] = &td_col_x_context;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].val.str_val = TPS_STRZ(td->x_context);
		nr_keys++;
	}

	if(_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.insert(_tps_db_handle, db_keys, db_vals, nr_keys) < 0) {
		LM_ERR("failed to store message\n");
		goto error;
	}

	return 0;

error:
	return -1;
}

/**
 *
 */
int tps_db_clean_dialogs(void)
{
	db_key_t db_keys[2];
	db_val_t db_vals[2];
	db_op_t db_ops[2];
	int nr_keys;

	if(_tps_db_handle == NULL) {
		LM_ERR("No database handle - misconfiguration?\n");
		return -1;
	}

	nr_keys = 0;

	LM_DBG("cleaning expired dialog records\n");

	db_keys[nr_keys] = &td_col_rectime;
	db_ops[nr_keys] = OP_LEQ;
	db_vals[nr_keys].type = DB1_DATETIME;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.time_val = time(NULL) - _tps_dialog_expire;
	nr_keys++;

	if(_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.delete(_tps_db_handle, db_keys, db_ops, db_vals, nr_keys) < 0) {
		LM_DBG("failed to clean expired dialog records\n");
	}

	/* dialog not confirmed - delete dlg after branch expires */
	db_vals[0].val.time_val = time(NULL) - _tps_branch_expire;

	db_keys[nr_keys] = &td_col_iflags;
	db_ops[nr_keys] = OP_EQ;
	db_vals[nr_keys].type = DB1_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = 0;
	nr_keys++;

	if(_tpsdbf.delete(_tps_db_handle, db_keys, db_ops, db_vals, nr_keys) < 0) {
		LM_DBG("failed to clean expired dialog records\n");
	}

	return 0;
}

/**
 *
 */
int tps_db_insert_branch(tps_data_t *td)
{
	db_key_t db_keys[TPS_NR_KEYS];
	db_val_t db_vals[TPS_NR_KEYS];
	int nr_keys;

	if(_tps_db_handle == NULL) {
		LM_ERR("No database handle - misconfiguration?\n");
		goto error;
	}

	memset(db_keys, 0, TPS_NR_KEYS * sizeof(db_key_t));
	memset(db_vals, 0, TPS_NR_KEYS * sizeof(db_val_t));
	nr_keys = 0;

	db_keys[nr_keys] = &tt_col_rectime;
	db_vals[nr_keys].type = DB1_DATETIME;
	db_vals[nr_keys].val.time_val = time(NULL);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_a_callid;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_callid);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_a_uuid;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_uuid);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_b_uuid;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->b_uuid);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_direction;
	db_vals[nr_keys].type = DB1_INT;
	db_vals[nr_keys].val.int_val = td->direction;
	nr_keys++;

	db_keys[nr_keys] = &tt_col_x_via;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->x_via);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_x_vbranch;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->x_vbranch1);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_x_rr;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->x_rr);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_y_rr;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->y_rr);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_s_rr;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->s_rr);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_x_uri;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->x_uri);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_x_tag;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->x_tag);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_s_method;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->s_method);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_s_cseq;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->s_cseq);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_a_contact;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_contact);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_b_contact;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->b_contact);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_as_contact;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->as_contact);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_bs_contact;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->bs_contact);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_a_tag;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->a_tag);
	nr_keys++;

	db_keys[nr_keys] = &tt_col_b_tag;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].val.str_val = TPS_STRZ(td->b_tag);
	nr_keys++;

	if(td->x_context.len > 0) {
		db_keys[nr_keys] = &tt_col_x_context;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].val.str_val = TPS_STRZ(td->x_context);
		nr_keys++;
	}

	if(_tpsdbf.use_table(_tps_db_handle, &tt_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.insert(_tps_db_handle, db_keys, db_vals, nr_keys) < 0) {
		LM_ERR("failed to store message\n");
		goto error;
	}

	return 0;

error:
	return -1;
}

/**
 *
 */
int tps_db_clean_branches(void)
{
	db_key_t db_keys[2];
	db_val_t db_vals[2];
	db_op_t db_ops[2];
	int nr_keys;

	if(_tps_db_handle == NULL) {
		LM_ERR("No database handle - misconfiguration?\n");
		return -1;
	}

	nr_keys = 0;

	LM_DBG("cleaning expired branch records\n");

	db_keys[nr_keys] = &tt_col_rectime;
	db_ops[nr_keys] = OP_LEQ;
	db_vals[nr_keys].type = DB1_DATETIME;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.time_val = time(NULL) - _tps_branch_expire;
	nr_keys++;

	if(_tpsdbf.use_table(_tps_db_handle, &tt_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.delete(_tps_db_handle, db_keys, db_ops, db_vals, nr_keys) < 0) {
		LM_DBG("failed to clean expired branch records\n");
	}
	return 0;
}

/**
 *
 */
#define TPS_DATA_APPEND_DB(_sd, _res, _c, _s)                                \
	do {                                                                     \
		if(RES_ROWS(_res)[0].values[_c].nul == 0) {                          \
			str tmp;                                                         \
			switch(RES_ROWS(_res)[0].values[_c].type) {                      \
				case DB1_STRING:                                             \
					tmp.s = (char *)RES_ROWS(_res)[0]                        \
									.values[_c]                              \
									.val.string_val;                         \
					if(tmp.s) {                                              \
						tmp.len = strlen(tmp.s);                             \
					} else {                                                 \
						tmp.len = 0;                                         \
					}                                                        \
					break;                                                   \
				case DB1_STR:                                                \
					tmp.len = RES_ROWS(_res)[0].values[_c].val.str_val.len;  \
					tmp.s = (char *)RES_ROWS(_res)[0]                        \
									.values[_c]                              \
									.val.str_val.s;                          \
					break;                                                   \
				case DB1_BLOB:                                               \
					tmp.len = RES_ROWS(_res)[0].values[_c].val.blob_val.len; \
					tmp.s = (char *)RES_ROWS(_res)[0]                        \
									.values[_c]                              \
									.val.blob_val.s;                         \
					break;                                                   \
				default:                                                     \
					tmp.len = 0;                                             \
					tmp.s = NULL;                                            \
			}                                                                \
			if((_sd)->cp + tmp.len >= (_sd)->cbuf + TPS_DATA_SIZE) {         \
				LM_ERR("not enough space for %d\n", _c);                     \
				goto error;                                                  \
			}                                                                \
			if(tmp.len > 0) {                                                \
				(_s)->s = (_sd)->cp;                                         \
				(_s)->len = tmp.len;                                         \
				memcpy((_sd)->cp, tmp.s, tmp.len);                           \
				(_sd)->cp += tmp.len;                                        \
				(_sd)->cp[0] = '\0';                                         \
				(_sd)->cp++;                                                 \
			}                                                                \
		}                                                                    \
	} while(0);

#define TPS_DATA_APPEND_DB_R(_sd, _res, _r, _c, _s)                            \
	do {                                                                       \
		if(RES_ROWS(_res)[(_r)].values[_c].nul == 0) {                         \
			str tmp;                                                           \
			switch(RES_ROWS(_res)[(_r)].values[_c].type) {                     \
				case DB1_STRING:                                               \
					tmp.s = (char *)RES_ROWS(_res)[(_r)]                       \
									.values[_c]                                \
									.val.string_val;                           \
					if(tmp.s) {                                                \
						tmp.len = strlen(tmp.s);                               \
					} else {                                                   \
						tmp.len = 0;                                           \
					}                                                          \
					break;                                                     \
				case DB1_STR:                                                  \
					tmp.len = RES_ROWS(_res)[(_r)].values[_c].val.str_val.len; \
					tmp.s = (char *)RES_ROWS(_res)[(_r)]                       \
									.values[_c]                                \
									.val.str_val.s;                            \
					break;                                                     \
				case DB1_BLOB:                                                 \
					tmp.len =                                                  \
							RES_ROWS(_res)[(_r)].values[_c].val.blob_val.len;  \
					tmp.s = (char *)RES_ROWS(_res)[(_r)]                       \
									.values[_c]                                \
									.val.blob_val.s;                           \
					break;                                                     \
				default:                                                       \
					tmp.len = 0;                                               \
					tmp.s = NULL;                                              \
			}                                                                  \
			if((_sd)->cp + tmp.len >= (_sd)->cbuf + TPS_DATA_SIZE) {           \
				LM_ERR("not enough space for %d\n", _c);                       \
				goto error;                                                    \
			}                                                                  \
			if(tmp.len > 0) {                                                  \
				(_s)->s = (_sd)->cp;                                           \
				(_s)->len = tmp.len;                                           \
				memcpy((_sd)->cp, tmp.s, tmp.len);                             \
				(_sd)->cp += tmp.len;                                          \
				(_sd)->cp[0] = '\0';                                           \
				(_sd)->cp++;                                                   \
			}                                                                  \
		}                                                                      \
	} while(0);

/**
 *
 */
int tps_db_load_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode)
{
	db_key_t db_keys[5];
	db_op_t db_ops[5];
	db_val_t db_vals[5];
	db_key_t db_cols[TPS_NR_KEYS];
	db1_res_t *db_res = NULL;
	str sMethodDlg = str_init("INVITE");
	int nr_keys;
	int nr_cols;
	int n;
	int ret = 0;

	if(msg == NULL || md == NULL || sd == NULL || _tps_db_handle == NULL)
		return -1;

	nr_keys = 0;
	nr_cols = 0;

	if(get_cseq(msg)->method_id == METHOD_SUBSCRIBE) {
		sMethodDlg.s = "SUBSCRIBE";
		sMethodDlg.len = 9;
	} else if(get_cseq(msg)->method_id == METHOD_REGISTER) {
		sMethodDlg.s = "REGISTER";
		sMethodDlg.len = 8;
	} else if(get_cseq(msg)->method_id == METHOD_PUBLISH) {
		sMethodDlg.s = "PUBLISH";
		sMethodDlg.len = 7;
	} else if(get_cseq(msg)->method_id == METHOD_NOTIFY) {
		/* NOTIFY can be also sent during call setup - ignore dialog method */
		sMethodDlg.s = "";
		sMethodDlg.len = 0;
	}

	if(mode == 0) {
		/* load same transaction using Via branch */
		db_keys[nr_keys] = &tt_col_x_vbranch;
		db_ops[nr_keys] = OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(md->x_vbranch1);
		nr_keys++;
	} else {
		/* load corresponding INVITE transaction using call-id + to-tag */
		db_keys[nr_keys] = &tt_col_a_callid;
		db_ops[nr_keys] = OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_callid);
		nr_keys++;

		db_keys[nr_keys] = &tt_col_b_tag;
		db_ops[nr_keys] = OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		if(md->direction == TPS_DIR_DOWNSTREAM) {
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->b_tag);
		} else {
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_tag);
		}
		nr_keys++;

		if(sMethodDlg.len > 0) {
			db_keys[nr_keys] = &tt_col_s_method;
			db_ops[nr_keys] = OP_EQ;
			db_vals[nr_keys].type = DB1_STR;
			db_vals[nr_keys].nul = 0;
			db_vals[nr_keys].val.str_val = sMethodDlg;
			nr_keys++;
		}

		if(md->a_uuid.len > 0) {
			if(md->a_uuid.s[0] == 'a') {
				db_keys[nr_keys] = &tt_col_a_uuid;
				db_ops[nr_keys] = OP_EQ;
				db_vals[nr_keys].type = DB1_STR;
				db_vals[nr_keys].nul = 0;
				db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_uuid);
				nr_keys++;
			} else if(md->a_uuid.s[0] == 'b') {
				db_keys[nr_keys] = &tt_col_b_uuid;
				db_ops[nr_keys] = OP_EQ;
				db_vals[nr_keys].type = DB1_STR;
				db_vals[nr_keys].nul = 0;
				db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_uuid);
				nr_keys++;
			}
		} else if(md->b_uuid.len > 0) {
			if(md->b_uuid.s[0] == 'a') {
				db_keys[nr_keys] = &tt_col_a_uuid;
				db_ops[nr_keys] = OP_EQ;
				db_vals[nr_keys].type = DB1_STR;
				db_vals[nr_keys].nul = 0;
				db_vals[nr_keys].val.str_val = TPS_STRZ(md->b_uuid);
				nr_keys++;
			} else if(md->b_uuid.s[0] == 'b') {
				db_keys[nr_keys] = &tt_col_b_uuid;
				db_ops[nr_keys] = OP_EQ;
				db_vals[nr_keys].type = DB1_STR;
				db_vals[nr_keys].nul = 0;
				db_vals[nr_keys].val.str_val = TPS_STRZ(md->b_uuid);
				nr_keys++;
			}
		}
	}

	if(msg->first_line.type == SIP_REQUEST && md->x_context.len > 0) {
		db_keys[nr_keys] = &tt_col_x_context;
		db_ops[nr_keys] = OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(md->x_context);
		nr_keys++;
	}

	db_cols[nr_cols++] = &tt_col_rectime;
	db_cols[nr_cols++] = &tt_col_a_callid;
	db_cols[nr_cols++] = &tt_col_a_uuid;
	db_cols[nr_cols++] = &tt_col_b_uuid;
	db_cols[nr_cols++] = &tt_col_direction;
	db_cols[nr_cols++] = &tt_col_x_via;
	db_cols[nr_cols++] = &tt_col_x_vbranch;
	db_cols[nr_cols++] = &tt_col_x_rr;
	db_cols[nr_cols++] = &tt_col_y_rr;
	db_cols[nr_cols++] = &tt_col_s_rr;
	db_cols[nr_cols++] = &tt_col_x_uri;
	db_cols[nr_cols++] = &tt_col_x_tag;
	db_cols[nr_cols++] = &tt_col_s_method;
	db_cols[nr_cols++] = &tt_col_s_cseq;
	db_cols[nr_cols++] = &tt_col_a_contact;
	db_cols[nr_cols++] = &tt_col_b_contact;
	db_cols[nr_cols++] = &tt_col_as_contact;
	db_cols[nr_cols++] = &tt_col_bs_contact;
	db_cols[nr_cols++] = &tt_col_a_tag;
	db_cols[nr_cols++] = &tt_col_b_tag;
	if(md->x_context.len > 0) {
		db_cols[nr_cols++] = &tt_col_x_context;
	}

	if(_tpsdbf.use_table(_tps_db_handle, &tt_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.query(_tps_db_handle, db_keys, db_ops, db_vals, db_cols, nr_keys,
			   nr_cols, NULL, &db_res)
			< 0) {
		LM_ERR("failed to query database\n");
		goto error;
	}

	if(RES_ROW_N(db_res) <= 0) {
		if(mode == 0) {
			LM_DBG("no stored record for <%.*s>\n", md->x_vbranch1.len,
					ZSW(md->x_vbranch1.s));
		} else {
			LM_DBG("no stored record for %.*s <%.*s ~ %.*s>\n", sMethodDlg.len,
					ZSW(sMethodDlg.s), md->a_callid.len, ZSW(md->a_callid.s),
					md->b_tag.len, ZSW(md->b_tag.s));
		}
		ret = 1;
		goto done;
	}

	sd->cp = sd->cbuf;

	n = 0;
	n++; /*rectime*/
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_callid);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_uuid);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_uuid);
	n++;
	n++; /*direction*/
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_via);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_vbranch1);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_rr);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->y_rr);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_rr);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_uri);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_tag);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_method);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_cseq);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_contact);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_contact);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->as_contact);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->bs_contact);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_tag);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_tag);
	n++;
	if(md->x_context.len > 0) {
		TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_context);
		n++;
	}

done:
	if((db_res != NULL) && _tpsdbf.free_result(_tps_db_handle, db_res) < 0)
		LM_ERR("failed to free result of query\n");

	return ret;

error:
	if((db_res != NULL) && _tpsdbf.free_result(_tps_db_handle, db_res) < 0)
		LM_ERR("failed to free result of query\n");

	return -1;
}

/**
 *
 */
int tps_storage_load_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode)
{
	return _tps_storage_api.load_branch(msg, md, sd, mode);
}

/**
 *
 */
int tps_db_load_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	db_key_t db_keys[5];
	db_op_t db_ops[5];
	db_val_t db_vals[5];
	db_key_t db_cols[TPS_NR_KEYS];
	db1_res_t *db_res = NULL;
	int nr_keys;
	int nr_cols;
	int n;
	int ret = 0;

	if(msg == NULL || md == NULL || sd == NULL || _tps_db_handle == NULL)
		return -1;

	if(md->a_uuid.len <= 0 && md->b_uuid.len <= 0) {
		LM_DBG("no dlg uuid provided\n");
		return -1;
	}

	nr_keys = 0;
	nr_cols = 0;

	db_ops[nr_keys] = OP_EQ;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.len = 0;
	if(md->a_uuid.len > 0) {
		if(md->a_uuid.s[0] == 'a') {
			db_keys[nr_keys] = &td_col_a_uuid;
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_uuid);
		} else if(md->a_uuid.s[0] == 'b') {
			db_keys[nr_keys] = &td_col_b_uuid;
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_uuid);
		}
	} else {
		if(md->b_uuid.s[0] == 'a') {
			db_keys[nr_keys] = &td_col_a_uuid;
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->b_uuid);
		} else if(md->b_uuid.s[0] == 'b') {
			db_keys[nr_keys] = &td_col_b_uuid;
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->b_uuid);
		}
	}
	if(db_vals[nr_keys].val.str_val.len <= 0) {
		LM_ERR("invalid dlg uuid provided\n");
		return -1;
	}
	nr_keys++;

	if(md->x_context.len > 0) {
		db_keys[nr_keys] = &td_col_x_context;
		db_ops[nr_keys] = OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(md->x_context);
		nr_keys++;
	}

	db_cols[nr_cols++] = &td_col_rectime;
	db_cols[nr_cols++] = &td_col_a_callid;
	db_cols[nr_cols++] = &td_col_a_uuid;
	db_cols[nr_cols++] = &td_col_b_uuid;
	db_cols[nr_cols++] = &td_col_a_contact;
	db_cols[nr_cols++] = &td_col_b_contact;
	db_cols[nr_cols++] = &td_col_as_contact;
	db_cols[nr_cols++] = &td_col_bs_contact;
	db_cols[nr_cols++] = &td_col_a_tag;
	db_cols[nr_cols++] = &td_col_b_tag;
	db_cols[nr_cols++] = &td_col_a_rr;
	db_cols[nr_cols++] = &td_col_b_rr;
	db_cols[nr_cols++] = &td_col_s_rr;
	db_cols[nr_cols++] = &td_col_iflags;
	db_cols[nr_cols++] = &td_col_a_uri;
	db_cols[nr_cols++] = &td_col_b_uri;
	db_cols[nr_cols++] = &td_col_r_uri;
	db_cols[nr_cols++] = &td_col_a_srcaddr;
	db_cols[nr_cols++] = &td_col_b_srcaddr;
	db_cols[nr_cols++] = &td_col_s_method;
	db_cols[nr_cols++] = &td_col_s_cseq;
	if(md->x_context.len > 0) {
		db_cols[nr_cols++] = &td_col_x_context;
	}

	if(_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.query(_tps_db_handle, db_keys, db_ops, db_vals, db_cols, nr_keys,
			   nr_cols, NULL, &db_res)
			< 0) {
		LM_ERR("failed to query database\n");
		goto error;
	}

	if(RES_ROW_N(db_res) <= 0) {
		LM_DBG("no stored record for <%.*s>\n", md->a_uuid.len,
				ZSW(md->a_uuid.s));
		ret = 1;
		goto done;
	}

	sd->cp = sd->cbuf;

	n = 0;
	n++; /*rectime*/
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_callid);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_uuid);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_uuid);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_contact);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_contact);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->as_contact);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->bs_contact);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_tag);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_tag);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_rr);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_rr);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_rr);
	n++;
	n++; /*iflags*/
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_uri);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_uri);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->r_uri);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_srcaddr);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_srcaddr);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_method);
	n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_cseq);
	n++;
	if(md->x_context.len > 0) {
		TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_context);
		n++;
	}

done:
	if((db_res != NULL) && _tpsdbf.free_result(_tps_db_handle, db_res) < 0)
		LM_ERR("failed to free result of query\n");

	return ret;

error:
	if((db_res != NULL) && _tpsdbf.free_result(_tps_db_handle, db_res) < 0)
		LM_ERR("failed to free result of query\n");

	return -1;
}

/**
 * Load active reg/pub/sub state by call-id (+ optional tags for SUBSCRIBE)
 */
int tps_db_load_dialog_by_tags(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	db_key_t db_keys[6];
	db_op_t db_ops[6];
	db_val_t db_vals[6];
	db_key_t db_cols[TPS_NR_KEYS];
	db1_res_t *db_res = NULL;
	int nr_keys;
	int nr_cols;
	int n;
	int ret = 0;

	if(msg == NULL || md == NULL || sd == NULL || _tps_db_handle == NULL)
		return -1;

	if(md->s_method.len <= 0) {
		LM_DBG("no method for tag lookup\n");
		return 1;
	}

	nr_keys = 0;
	nr_cols = 0;

	if(md->s_method_id == METHOD_PUBLISH && md->b_uri.len > 0) {
		/* PUBLISH refresh by SIP-If-Match (stored in b_uri, RFC 3903) */
		db_keys[nr_keys] = &td_col_b_uri;
		db_ops[nr_keys] = OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(md->b_uri);
		nr_keys++;
	} else {
		if(md->a_callid.len <= 0) {
			LM_DBG("no call-id for tag lookup\n");
			return 1;
		}
		db_keys[nr_keys] = &td_col_a_callid;
		db_ops[nr_keys] = OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_callid);
		nr_keys++;
	}

	/* REGISTER/PUBLISH: correlate by Call-ID + method only (RFC 3261/3903) */
	if(!tps_data_is_reg_pub(md->s_method_id)) {
		if(md->a_tag.len > 0) {
			db_keys[nr_keys] = &td_col_a_tag;
			db_ops[nr_keys] = OP_EQ;
			db_vals[nr_keys].type = DB1_STR;
			db_vals[nr_keys].nul = 0;
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_tag);
			nr_keys++;
		}

		if(md->b_tag.len > 0) {
			db_keys[nr_keys] = &td_col_b_tag;
			db_ops[nr_keys] = OP_EQ;
			db_vals[nr_keys].type = DB1_STR;
			db_vals[nr_keys].nul = 0;
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->b_tag);
			nr_keys++;
		}
	}

	db_keys[nr_keys] = &td_col_s_method;
	db_ops[nr_keys] = OP_EQ;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val = TPS_STRZ(md->s_method);
	nr_keys++;

	db_keys[nr_keys] = &td_col_iflags;
	db_ops[nr_keys] = OP_GT;
	db_vals[nr_keys].type = DB1_INT;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.int_val = 0;
	nr_keys++;

	db_cols[nr_cols++] = &td_col_rectime;
	db_cols[nr_cols++] = &td_col_a_callid;
	db_cols[nr_cols++] = &td_col_a_uuid;
	db_cols[nr_cols++] = &td_col_b_uuid;
	db_cols[nr_cols++] = &td_col_a_contact;
	db_cols[nr_cols++] = &td_col_b_contact;
	db_cols[nr_cols++] = &td_col_as_contact;
	db_cols[nr_cols++] = &td_col_bs_contact;
	db_cols[nr_cols++] = &td_col_a_tag;
	db_cols[nr_cols++] = &td_col_b_tag;
	db_cols[nr_cols++] = &td_col_a_rr;
	db_cols[nr_cols++] = &td_col_b_rr;
	db_cols[nr_cols++] = &td_col_s_rr;
	db_cols[nr_cols++] = &td_col_iflags;
	db_cols[nr_cols++] = &td_col_a_uri;
	db_cols[nr_cols++] = &td_col_b_uri;
	db_cols[nr_cols++] = &td_col_r_uri;
	db_cols[nr_cols++] = &td_col_a_srcaddr;
	db_cols[nr_cols++] = &td_col_b_srcaddr;
	db_cols[nr_cols++] = &td_col_s_method;
	db_cols[nr_cols++] = &td_col_s_cseq;
	if(md->x_context.len > 0) {
		db_cols[nr_cols++] = &td_col_x_context;
	}

	if(_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.query(_tps_db_handle, db_keys, db_ops, db_vals, db_cols, nr_keys,
			   nr_cols, &td_col_rectime, &db_res)
			< 0) {
		LM_ERR("failed to query database\n");
		goto error;
	}

	if(RES_ROW_N(db_res) <= 0) {
		LM_DBG("no active dialog for <%.*s ~ %.*s ~ %.*s>\n", md->a_callid.len,
				ZSW(md->a_callid.s), md->a_tag.len, ZSW(md->a_tag.s),
				md->b_tag.len, ZSW(md->b_tag.s));
		ret = 1;
		goto done;
	}

	{
		int row = 0;
		int i;
		time_t best_t = 0;
		time_t t;

		if(RES_ROW_N(db_res) > 1) {
			for(i = 0; i < RES_ROW_N(db_res); i++) {
				if(RES_ROWS(db_res)[i].values[0].type == DB1_DATETIME) {
					t = RES_ROWS(db_res)[i].values[0].val.time_val;
					if(t >= best_t) {
						best_t = t;
						row = i;
					}
				}
			}
			LM_WARN("multiple active %.*s rows, using newest (rectime)\n",
					md->s_method.len, md->s_method.s);
		}

		sd->cp = sd->cbuf;

		n = 0;
		n++; /*rectime*/
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->a_callid);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->a_uuid);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->b_uuid);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->a_contact);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->b_contact);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->as_contact);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->bs_contact);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->a_tag);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->b_tag);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->a_rr);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->b_rr);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->s_rr);
		n++;
		n++; /*iflags*/
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->a_uri);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->b_uri);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->r_uri);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->a_srcaddr);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->b_srcaddr);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->s_method);
		n++;
		TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->s_cseq);
		n++;
		if(md->x_context.len > 0) {
			TPS_DATA_APPEND_DB_R(sd, db_res, row, n, &sd->x_context);
			n++;
		}
	}

done:
	if((db_res != NULL) && _tpsdbf.free_result(_tps_db_handle, db_res) < 0)
		LM_ERR("failed to free result of query\n");

	return ret;

error:
	if((db_res != NULL) && _tpsdbf.free_result(_tps_db_handle, db_res) < 0)
		LM_ERR("failed to free result of query\n");

	return -1;
}

/**
 *
 */
int tps_storage_load_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	return _tps_storage_api.load_dialog(msg, md, sd);
}

/**
 *
 */
int tps_storage_load_dialog_by_tags(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	if(_tps_storage_api.load_dialog_by_tags == NULL) {
		return 1;
	}
	return _tps_storage_api.load_dialog_by_tags(msg, md, sd);
}

#define TPS_DB_ADD_STRV(dcol, dval, cnr, cname, cval) \
	do {                                              \
		if(cval.len > 0) {                            \
			dcol[cnr] = &cname;                       \
			dval[cnr].type = DB1_STR;                 \
			dval[cnr].val.str_val = TPS_STRZ(cval);   \
			cnr++;                                    \
		}                                             \
	} while(0)

/**
 *
 */
int tps_db_update_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode)
{
	db_key_t db_keys[8];
	db_op_t db_ops[8];
	db_val_t db_vals[8];
	db_key_t db_ucols[TPS_NR_KEYS];
	db_val_t db_uvals[TPS_NR_KEYS];
	int nr_keys;
	int nr_ucols;

	if(_tps_db_handle == NULL)
		return -1;

	memset(db_ucols, 0, TPS_NR_KEYS * sizeof(db_key_t));
	memset(db_uvals, 0, TPS_NR_KEYS * sizeof(db_val_t));

	nr_keys = 0;
	nr_ucols = 0;

	db_keys[nr_keys] = &tt_col_a_uuid;
	db_ops[nr_keys] = OP_EQ;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	if(sd->a_uuid.len > 0 && sd->a_uuid.s[0] == 'a') {
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->a_uuid);
	} else {
		if(sd->b_uuid.len <= 0) {
			LM_ERR("no valid dlg uuid\n");
			return -1;
		}
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->b_uuid);
	}
	nr_keys++;

	if(sd->x_context.len > 0) {
		db_keys[nr_keys] = &tt_col_x_context;
		db_ops[nr_keys] = OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->x_context);
		nr_keys++;
	}

	if(mode & TPS_DBU_CONTACT) {
		TPS_DB_ADD_STRV(
				db_ucols, db_uvals, nr_ucols, tt_col_a_contact, md->a_contact);
		TPS_DB_ADD_STRV(
				db_ucols, db_uvals, nr_ucols, tt_col_b_contact, md->b_contact);
	}

	if((mode & TPS_DBU_RPLATTRS) && msg->first_line.type == SIP_REPLY) {
		if(msg->first_line.u.reply.statuscode >= 180
				&& msg->first_line.u.reply.statuscode < 200) {

			db_ucols[nr_ucols] = &tt_col_y_rr;
			db_uvals[nr_ucols].type = DB1_STR;
			db_uvals[nr_ucols].val.str_val = TPS_STRZ(md->b_rr);
			nr_ucols++;

			TPS_DB_ADD_STRV(
					db_ucols, db_uvals, nr_ucols, tt_col_b_tag, md->b_tag);
		}
	}
	if(nr_ucols == 0) {
		return 0;
	}
	if(_tpsdbf.use_table(_tps_db_handle, &tt_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.update(_tps_db_handle, db_keys, db_ops, db_vals, db_ucols,
			   db_uvals, nr_keys, nr_ucols)
			!= 0) {
		LM_ERR("failed to do branch db update for [%.*s]!\n", md->a_uuid.len,
				md->a_uuid.s);
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_storage_update_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode)
{
	int ret;

	if(msg == NULL || md == NULL || sd == NULL)
		return -1;

	if((md->s_method_id != METHOD_INVITE)
			&& (md->s_method_id != METHOD_SUBSCRIBE)
			&& (md->s_method_id != METHOD_REGISTER)
			&& (md->s_method_id != METHOD_PUBLISH)) {
		return 0;
	}

	if(msg->first_line.type == SIP_REPLY) {
		if(msg->first_line.u.reply.statuscode < 180
				|| msg->first_line.u.reply.statuscode >= 200) {
			return 0;
		}
	}

	ret = tps_storage_link_msg(msg, md, md->direction);
	if(ret < 0)
		return -1;

	return _tps_storage_api.update_branch(msg, md, sd, mode);
}

/**
 *
 */
int tps_db_update_dialog(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode)
{
	db_key_t db_keys[8];
	db_op_t db_ops[8];
	db_val_t db_vals[8];
	db_key_t db_ucols[TPS_NR_KEYS];
	db_val_t db_uvals[TPS_NR_KEYS];
	int nr_keys;
	int nr_ucols;

	if(_tps_db_handle == NULL)
		return -1;

	memset(db_ucols, 0, TPS_NR_KEYS * sizeof(db_key_t));
	memset(db_uvals, 0, TPS_NR_KEYS * sizeof(db_val_t));

	nr_keys = 0;
	nr_ucols = 0;

	db_keys[nr_keys] = &td_col_a_uuid;
	db_ops[nr_keys] = OP_EQ;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	if(sd->a_uuid.len > 0 && sd->a_uuid.s[0] == 'a') {
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->a_uuid);
	} else {
		if(sd->b_uuid.len <= 0) {
			LM_ERR("no valid dlg uuid (%d:%.*s - %d:%.*s)\n", sd->a_uuid.len,
					sd->a_uuid.len, ZSW(sd->a_uuid.s), sd->b_uuid.len,
					sd->b_uuid.len, ZSW(sd->b_uuid.s));
			return -1;
		}
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->b_uuid);
	}
	nr_keys++;

	if(sd->x_context.len > 0) {
		db_keys[nr_keys] = &td_col_x_context;
		db_ops[nr_keys] = OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->x_context);
		nr_keys++;
	}

	if(mode & TPS_DBU_CONTACT) {
		TPS_DB_ADD_STRV(
				db_ucols, db_uvals, nr_ucols, td_col_a_contact, md->a_contact);
		TPS_DB_ADD_STRV(
				db_ucols, db_uvals, nr_ucols, td_col_b_contact, md->b_contact);
	}

	if((mode & TPS_DBU_RPLATTRS) && msg->first_line.type == SIP_REPLY) {
		if(sd->b_tag.len <= 0 && msg->first_line.u.reply.statuscode >= 200
				&& msg->first_line.u.reply.statuscode < 300) {

			if((sd->iflags & TPS_IFLAG_DLGON) == 0) {
				db_ucols[nr_ucols] = &td_col_b_rr;
				db_uvals[nr_ucols].type = DB1_STR;
				db_uvals[nr_ucols].val.str_val = TPS_STRZ(md->b_rr);
				nr_ucols++;
			}

			TPS_DB_ADD_STRV(
					db_ucols, db_uvals, nr_ucols, td_col_b_tag, md->b_tag);

			db_ucols[nr_ucols] = &td_col_iflags;
			db_uvals[nr_ucols].type = DB1_INT;
			db_uvals[nr_ucols].val.int_val = sd->iflags | TPS_IFLAG_DLGON;
			nr_ucols++;
		}
	}
	if(sd->b_tag.len > 0 && ((mode & TPS_DBU_BRR) || (mode & TPS_DBU_ARR))) {
		if(((md->direction == TPS_DIR_DOWNSTREAM)
				   && (msg->first_line.type == SIP_REPLY))
				|| ((md->direction == TPS_DIR_UPSTREAM)
						&& (msg->first_line.type == SIP_REQUEST))) {
			if(((sd->iflags & TPS_IFLAG_DLGON) == 0) && (mode & TPS_DBU_BRR)) {
				db_ucols[nr_ucols] = &td_col_b_rr;
				db_uvals[nr_ucols].type = DB1_STR;
				db_uvals[nr_ucols].val.str_val = TPS_STRZ(md->b_rr);
				nr_ucols++;
			}
		} else {
			if(((sd->iflags & TPS_IFLAG_DLGON) == 0) && (mode & TPS_DBU_ARR)) {
				db_ucols[nr_ucols] = &td_col_a_rr;
				db_uvals[nr_ucols].type = DB1_STR;
				db_uvals[nr_ucols].val.str_val = TPS_STRZ(md->a_rr);
				nr_ucols++;
				db_ucols[nr_ucols] = &td_col_s_rr;
				db_uvals[nr_ucols].type = DB1_STR;
				db_uvals[nr_ucols].val.str_val = TPS_STRZ(md->s_rr);
				nr_ucols++;
			}
		}
	}
	if((mode & TPS_DBU_TIME) && (msg->first_line.type == SIP_REQUEST)
			&& (msg->first_line.u.request.method_value
					& _tps_methods_update_time)
			&& (sd->b_tag.len > 0 || tps_data_is_reg_pub(md->s_method_id))) {
		db_ucols[nr_ucols] = &td_col_rectime;
		db_uvals[nr_ucols].type = DB1_DATETIME;
		db_uvals[nr_ucols].val.time_val = time(NULL);
		nr_ucols++;
	}

	if((mode & TPS_DBU_PUBETAG) && sd->b_uri.len > 0) {
		TPS_DB_ADD_STRV(db_ucols, db_uvals, nr_ucols, td_col_b_uri, sd->b_uri);
	}

	if(nr_ucols == 0) {
		return 0;
	}
	if(_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.update(_tps_db_handle, db_keys, db_ops, db_vals, db_ucols,
			   db_uvals, nr_keys, nr_ucols)
			!= 0) {
		LM_ERR("failed to do dialog db update for [%.*s]!\n", md->a_uuid.len,
				md->a_uuid.s);
		return -1;
	}
	return 0;
}

/**
 *
 */
int tps_storage_update_dialog(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode)
{
	int ret;

	if(msg == NULL || md == NULL || sd == NULL)
		return -1;

	if((md->s_method_id != METHOD_INVITE)
			&& (md->s_method_id != METHOD_SUBSCRIBE)
			&& (md->s_method_id != METHOD_REGISTER)
			&& (md->s_method_id != METHOD_PUBLISH)) {
		return 0;
	}
	if(msg->first_line.type == SIP_REPLY) {
		if(msg->first_line.u.reply.statuscode < 200
				|| msg->first_line.u.reply.statuscode >= 300) {
			return 0;
		}
	}

	ret = tps_storage_link_msg(msg, md, md->direction);
	if(ret < 0)
		return -1;

	return _tps_storage_api.update_dialog(msg, md, sd, mode);
}

/**
 *
 */
int tps_db_end_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	db_key_t db_keys[4];
	db_op_t db_ops[4];
	db_val_t db_vals[4];
	db_key_t db_ucols[TPS_NR_KEYS];
	db_val_t db_uvals[TPS_NR_KEYS];
	int nr_keys;
	int nr_ucols;

	if(msg == NULL || md == NULL || sd == NULL || _tps_db_handle == NULL)
		return -1;

	if(!tps_data_end_dialog_match(msg, md)) {
		return 0;
	}

	memset(db_ucols, 0, TPS_NR_KEYS * sizeof(db_key_t));
	memset(db_uvals, 0, TPS_NR_KEYS * sizeof(db_val_t));

	nr_keys = 0;
	nr_ucols = 0;

	db_keys[nr_keys] = &td_col_a_uuid;
	db_ops[nr_keys] = OP_EQ;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	if(sd->a_uuid.len > 0 && sd->a_uuid.s[0] == 'a') {
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->a_uuid);
	} else {
		if(sd->b_uuid.len <= 0) {
			LM_ERR("no valid dlg uuid\n");
			return -1;
		}
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->b_uuid);
	}
	nr_keys++;

	if(sd->x_context.len > 0) {
		db_keys[nr_keys] = &td_col_x_context;
		db_ops[nr_keys] = OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->x_context);
		nr_keys++;
	}

	db_ucols[nr_ucols] = &td_col_rectime;
	db_uvals[nr_ucols].type = DB1_DATETIME;
	db_uvals[nr_ucols].val.time_val = time(NULL);
	nr_ucols++;

	db_ucols[nr_ucols] = &td_col_iflags;
	db_uvals[nr_ucols].type = DB1_INT;
	db_uvals[nr_ucols].val.int_val = 0;
	nr_ucols++;

	if(_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.update(_tps_db_handle, db_keys, db_ops, db_vals, db_ucols,
			   db_uvals, nr_keys, nr_ucols)
			!= 0) {
		LM_ERR("failed to do db update for [%.*s]!\n", md->a_uuid.len,
				md->a_uuid.s);
		return -1;
	}
	LM_DBG("ended dialog storage\n");
	return 0;
}

/**
 *
 */
int tps_storage_end_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	return _tps_storage_api.end_dialog(msg, md, sd);
}

/**
 *
 */
void tps_storage_clean(unsigned int ticks, void *param)
{
	_tps_storage_api.clean_branches();
	_tps_storage_api.clean_dialogs();
}
