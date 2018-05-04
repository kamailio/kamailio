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
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"

#include "../../lib/srdb1/db.h"
#include "../../lib/srutils/sruid.h"

#include "tps_storage.h"
#include "api.h"

extern sruid_t _tps_sruid;

extern db1_con_t* _tps_db_handle;
extern db_func_t _tpsdbf;

#define TPS_STORAGE_LOCK_SIZE	1<<9
static gen_lock_set_t *_tps_storage_lock_set = NULL;

int _tps_branch_expire = 180;
int _tps_dialog_expire = 10800;

int tps_db_insert_dialog(tps_data_t *td);
int tps_db_clean_dialogs(void);
int tps_db_insert_branch(tps_data_t *td);
int tps_db_clean_branches(void);
int tps_db_load_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode);
int tps_db_load_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);
int tps_db_update_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode);
int tps_db_update_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode);
int tps_db_end_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);

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
	.update_branch = tps_db_update_branch,
	.update_dialog = tps_db_update_dialog,
	.end_dialog = tps_db_end_dialog
};

/**
 *
 */
int tps_set_storage_api(tps_storage_api_t *tsa)
{
	if(tsa==NULL)
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
	if(_tps_storage_lock_set==NULL
			|| lock_set_init(_tps_storage_lock_set)==NULL) {
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
	if(_tps_storage_lock_set!=NULL) {
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
int tps_storage_fill_contact(sip_msg_t *msg, tps_data_t *td, str *uuid, int dir)
{
	str sv;
	sip_uri_t puri;
	int i;

	if(dir==TPS_DIR_DOWNSTREAM) {
		sv = td->bs_contact;
	} else {
		sv = td->as_contact;
	}
	if(sv.len<=0) {
		/* no contact - skip */
		return 0;
	}

	if(td->cp + 8 + (2*uuid->len) + sv.len >= td->cbuf + TPS_DATA_SIZE) {
		LM_ERR("insufficient data buffer\n");
		return -1;
	}
	if (parse_uri(sv.s, sv.len, &puri) < 0) {
		LM_ERR("failed to parse the uri\n");
		return -1;
	}
	if(dir==TPS_DIR_DOWNSTREAM) {
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
	*td->cp = '<';
	td->cp++;
	for(i=0; i<sv.len; i++) {
		*td->cp = sv.s[i];
		td->cp++;
		if(sv.s[i]==':') break;
	}
	if(dir==TPS_DIR_DOWNSTREAM) {
		*td->cp = 'b';
	} else {
		*td->cp = 'a';
	}
	td->cp++;
	memcpy(td->cp, uuid->s, uuid->len);
	td->cp += uuid->len;
	*td->cp = '@';
	td->cp++;
	memcpy(td->cp, puri.host.s, puri.host.len);
	td->cp += puri.host.len;
	if(puri.port.len>0) {
		*td->cp = ':';
		td->cp++;
		memcpy(td->cp, puri.port.s, puri.port.len);
		td->cp += puri.port.len;
	}
	if(puri.transport_val.len>0) {
		memcpy(td->cp, ";transport=", 11);
		td->cp += 11;
		memcpy(td->cp, puri.transport_val.s, puri.transport_val.len);
		td->cp += puri.transport_val.len;
	}

	*td->cp = '>';
	td->cp++;
	if(dir==TPS_DIR_DOWNSTREAM) {
		td->bs_contact.len = td->cp - td->bs_contact.s;
	} else {
		td->as_contact.len = td->cp - td->as_contact.s;
	}
	return 0;
}

/**
 *
 */
int tps_storage_link_msg(sip_msg_t *msg, tps_data_t *td, int dir)
{
	str stxt;

	if(parse_headers(msg, HDR_EOH_F, 0)==-1)
		return -1;

	/* callid */
	stxt = msg->callid->body;
	trim(&stxt);
	td->a_callid = stxt;

	/* get from-tag */
	if(parse_from_header(msg)<0 || msg->from==NULL) {
		LM_ERR("failed getting 'from' header!\n");
		goto error;
	}
	td->a_tag = get_from(msg)->tag_value;

	/* get to-tag */
	if(parse_to_header(msg)<0 || msg->to==NULL) {
		LM_ERR("failed getting 'to' header!\n");
		goto error;
	}
	td->b_tag = get_to(msg)->tag_value;

	if(dir==TPS_DIR_DOWNSTREAM) {
		td->x_tag = td->a_tag;
	} else {
		td->x_tag = td->b_tag;
	}

	td->x_via = td->x_via2;
	if(parse_headers(msg, HDR_CSEQ_F, 0)!=0 || msg->cseq==NULL) {
		LM_ERR("cannot parse cseq header\n");
		return -1; /* should it be 0 ?!?! */
	}
	td->s_method = get_cseq(msg)->method;
	td->s_cseq = get_cseq(msg)->number;

	/* extract the contact address */
	if(parse_headers(msg, HDR_CONTACT_F, 0)<0 || msg->contact==NULL) {
		if(td->s_method_id != METHOD_INVITE) {
			/* no mandatory contact unless is INVITE - done */
			return 0;
		}
		LM_ERR("bad sip message or missing Contact hdr\n");
		goto error;
	}
	if(parse_contact(msg->contact)<0
			|| ((contact_body_t*)msg->contact->parsed)->contacts==NULL
			|| ((contact_body_t*)msg->contact->parsed)->contacts->next!=NULL) {
		LM_ERR("bad Contact header\n");
		return -1;
	}
	if(msg->first_line.type==SIP_REQUEST) {
		if(dir==TPS_DIR_DOWNSTREAM) {
			td->a_contact = ((contact_body_t*)msg->contact->parsed)->contacts->uri;
		} else {
			td->b_contact = ((contact_body_t*)msg->contact->parsed)->contacts->uri;
		}
	} else {
		if(dir==TPS_DIR_DOWNSTREAM) {
			td->b_contact = ((contact_body_t*)msg->contact->parsed)->contacts->uri;
		} else {
			td->a_contact = ((contact_body_t*)msg->contact->parsed)->contacts->uri;
		}
	}

	return 0;

error:
	return -1;
}

/**
 *
 */
int tps_storage_record(sip_msg_t *msg, tps_data_t *td, int dialog)
{
	int ret;
	str suid;

	if(dialog==0) {
		sruid_next(&_tps_sruid);
		suid = _tps_sruid.uid;
	} else {
		if(td->a_uuid.len>0) {
			suid = td->a_uuid;
		} else if(td->b_uuid.len>0) {
			suid = td->b_uuid;
		} else {
			goto error;
		}
		suid.s++;
		suid.len--;
	}

	ret = tps_storage_fill_contact(msg, td, &suid, TPS_DIR_DOWNSTREAM);
	if(ret<0) goto error;
	ret = tps_storage_fill_contact(msg, td, &suid, TPS_DIR_UPSTREAM);
	if(ret<0) goto error;

	ret = tps_storage_link_msg(msg, td, TPS_DIR_DOWNSTREAM);
	if(ret<0) goto error;
	if(td->as_contact.len <= 0 && td->bs_contact.len <= 0) {
		LM_WARN("no local address - do record routing for all initial requests\n");
	}
	if(dialog==0) {
		ret = _tps_storage_api.insert_dialog(td);
		if(ret<0) goto error;
	}
	ret = _tps_storage_api.insert_branch(td);
	if(ret<0) goto error;

	return 0;

error:
	LM_ERR("failed to store\n");
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

#define TPS_NR_KEYS	48

str _tps_empty = str_init("");

#define TPS_STRZ(_s) ((_s).s)?(_s):(_tps_empty)
/**
 *
 */
int tps_db_insert_dialog(tps_data_t *td)
{
	db_key_t db_keys[TPS_NR_KEYS];
	db_val_t db_vals[TPS_NR_KEYS];
	int nr_keys;

	if (_tps_db_handle == NULL) {
		LM_ERR("No database handle - misconfiguration?\n");
		goto error;
	}

	memset(db_keys, 0, TPS_NR_KEYS*sizeof(db_key_t));
	memset(db_vals, 0, TPS_NR_KEYS*sizeof(db_val_t));
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

	if (_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
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
	db_op_t  db_ops[2];
	int nr_keys;

	if (_tps_db_handle == NULL) {
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

	if (_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if (_tpsdbf.delete(_tps_db_handle, db_keys, db_ops, db_vals, nr_keys) < 0) {
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

	if (_tpsdbf.delete(_tps_db_handle, db_keys, db_ops, db_vals, nr_keys) < 0) {
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

	if (_tps_db_handle == NULL) {
		LM_ERR("No database handle - misconfiguration?\n");
		goto error;
	}

	memset(db_keys, 0, TPS_NR_KEYS*sizeof(db_key_t));
	memset(db_vals, 0, TPS_NR_KEYS*sizeof(db_val_t));
	nr_keys = 0;

	db_keys[nr_keys] = &tt_col_rectime;
	db_vals[nr_keys].type = DB1_DATETIME;
	db_vals[nr_keys].val.int_val = time(NULL);
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

	if (_tpsdbf.use_table(_tps_db_handle, &tt_table_name) < 0) {
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
	db_op_t  db_ops[2];
	int nr_keys;

	if (_tps_db_handle == NULL) {
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

	if (_tpsdbf.use_table(_tps_db_handle, &tt_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if (_tpsdbf.delete(_tps_db_handle, db_keys, db_ops, db_vals, nr_keys) < 0) {
		LM_DBG("failed to clean expired branch records\n");
	}
	return 0;
}

/**
 *
 */
#define TPS_DATA_APPEND_DB(_sd, _res, _c, _s)	\
	do { \
		if (RES_ROWS(_res)[0].values[_c].nul == 0) \
		{ \
			str tmp; \
			switch(RES_ROWS(_res)[0].values[_c].type) \
			{ \
			case DB1_STRING: \
				tmp.s=(char*)RES_ROWS(_res)[0].values[_c].val.string_val; \
				if(tmp.s) { \
					tmp.len=strlen(tmp.s); \
				} else { \
					tmp.len=0; \
				} \
				break; \
			case DB1_STR: \
				tmp.len=RES_ROWS(_res)[0].values[_c].val.str_val.len; \
				tmp.s=(char*)RES_ROWS(_res)[0].values[_c].val.str_val.s; \
				break; \
			case DB1_BLOB: \
				tmp.len=RES_ROWS(_res)[0].values[_c].val.blob_val.len; \
				tmp.s=(char*)RES_ROWS(_res)[0].values[_c].val.blob_val.s; \
				break; \
			default: \
				tmp.len=0; \
				tmp.s=NULL; \
			} \
			if((_sd)->cp + tmp.len >= (_sd)->cbuf + TPS_DATA_SIZE) { \
				LM_ERR("not enough space for %d\n", _c); \
				goto error; \
			} \
			if(tmp.len>0) { \
				(_s)->s = (_sd)->cp; \
				(_s)->len = tmp.len; \
				memcpy((_sd)->cp, tmp.s, tmp.len); \
				(_sd)->cp += tmp.len; \
				(_sd)->cp[0] = '\0'; \
				(_sd)->cp++; \
			} \
		} \
	} while(0);

/**
 *
 */
int tps_db_load_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode)
{
	db_key_t db_keys[4];
	db_op_t  db_ops[4];
	db_val_t db_vals[4];
	db_key_t db_cols[TPS_NR_KEYS];
	db1_res_t* db_res = NULL;
	str sinv = str_init("INVITE");
	int nr_keys;
	int nr_cols;
	int n;
	int ret = 0;

	if(msg==NULL || md==NULL || sd==NULL || _tps_db_handle==NULL)
		return -1;

	nr_keys = 0;
	nr_cols = 0;

	if(mode==0) {
		/* load same transaction using Via branch */
		db_keys[nr_keys]=&tt_col_x_vbranch;
		db_ops[nr_keys]=OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(md->x_vbranch1);
		nr_keys++;
	} else {
		/* load corresponding INVITE transaction using call-id + to-tag */
		db_keys[nr_keys]=&tt_col_a_callid;
		db_ops[nr_keys]=OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_callid);
		nr_keys++;

		db_keys[nr_keys]=&tt_col_b_tag;
		db_ops[nr_keys]=OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = TPS_STRZ(md->b_tag);
		nr_keys++;

		db_keys[nr_keys]=&tt_col_s_method;
		db_ops[nr_keys]=OP_EQ;
		db_vals[nr_keys].type = DB1_STR;
		db_vals[nr_keys].nul = 0;
		db_vals[nr_keys].val.str_val = sinv;
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

	if (_tpsdbf.use_table(_tps_db_handle, &tt_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if (_tpsdbf.query(_tps_db_handle, db_keys, db_ops, db_vals, db_cols,
				nr_keys, nr_cols, NULL, &db_res) < 0) {
		LM_ERR("failed to query database\n");
		goto error;
	}

	if (RES_ROW_N(db_res) <= 0) {
		if(mode==0) {
			LM_DBG("no stored record for <%.*s>\n",
					md->x_vbranch1.len, ZSW(md->x_vbranch1.s));
		} else {
			LM_DBG("no stored record for INVITE <%.*s ~ %.*s>\n",
					md->a_callid.len, ZSW(md->a_callid.s),
					md->b_tag.len, ZSW(md->b_tag.s));
		}
		ret = 1;
		goto done;
	}

	sd->cp = sd->cbuf;

	n = 0;
	n++; /*rectime*/
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_callid); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_uuid); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_uuid); n++;
	n++; /*direction*/
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_via); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_vbranch1); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_rr); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->y_rr); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_rr); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_uri); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->x_tag); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_method); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_cseq); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_contact); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_contact); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->as_contact); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->bs_contact); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_tag); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_tag); n++;

done:
	if ((db_res!=NULL) && _tpsdbf.free_result(_tps_db_handle, db_res)<0)
		LM_ERR("failed to free result of query\n");

	return ret;

error:
	if ((db_res!=NULL) && _tpsdbf.free_result(_tps_db_handle, db_res)<0)
		LM_ERR("failed to free result of query\n");

	return -1;
}

/**
 *
 */
int tps_storage_load_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode)
{
	return _tps_storage_api.load_branch(msg, md, sd, mode);
}

/**
 *
 */
int tps_db_load_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	db_key_t db_keys[4];
	db_op_t  db_ops[4];
	db_val_t db_vals[4];
	db_key_t db_cols[TPS_NR_KEYS];
	db1_res_t* db_res = NULL;
	int nr_keys;
	int nr_cols;
	int n;
	int ret = 0;

	if(msg==NULL || md==NULL || sd==NULL || _tps_db_handle==NULL)
		return -1;

	if(md->a_uuid.len<=0 && md->b_uuid.len<=0) {
		LM_DBG("no dlg uuid provided\n");
		return -1;
	}

	nr_keys = 0;
	nr_cols = 0;

	db_ops[nr_keys]=OP_EQ;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	db_vals[nr_keys].val.str_val.len = 0;
	if(md->a_uuid.len>0) {
		if(md->a_uuid.s[0]=='a') {
			db_keys[nr_keys]=&td_col_a_uuid;
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_uuid);
		} else if(md->a_uuid.s[0]=='b') {
			db_keys[nr_keys]=&td_col_b_uuid;
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->a_uuid);
		}
	} else {
		if(md->b_uuid.s[0]=='a') {
			db_keys[nr_keys]=&td_col_a_uuid;
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->b_uuid);
		} else if(md->b_uuid.s[0]=='b') {
			db_keys[nr_keys]=&td_col_b_uuid;
			db_vals[nr_keys].val.str_val = TPS_STRZ(md->b_uuid);
		}
	}
	if(db_vals[nr_keys].val.str_val.len<=0) {
		LM_ERR("invalid dlg uuid provided\n");
		return -1;
	}
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


	if (_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if (_tpsdbf.query(_tps_db_handle, db_keys, db_ops, db_vals, db_cols,
				nr_keys, nr_cols, NULL, &db_res) < 0) {
		LM_ERR("failed to query database\n");
		goto error;
	}

	if (RES_ROW_N(db_res) <= 0) {
		LM_DBG("no stored record for <%.*s>\n",
				md->a_uuid.len, ZSW(md->a_uuid.s));
		ret = 1;
		goto done;
	}

	sd->cp = sd->cbuf;

	n = 0;
	n++; /*rectime*/
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_callid); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_uuid); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_uuid); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_contact); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_contact); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->as_contact); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->bs_contact); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_tag); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_tag); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_rr); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_rr); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_rr); n++;
	n++; /*iflags*/
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_uri); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_uri); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->r_uri); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->a_srcaddr); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->b_srcaddr); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_method); n++;
	TPS_DATA_APPEND_DB(sd, db_res, n, &sd->s_cseq); n++;

done:
	if ((db_res!=NULL) && _tpsdbf.free_result(_tps_db_handle, db_res)<0)
		LM_ERR("failed to free result of query\n");

	return ret;

error:
	if ((db_res!=NULL) && _tpsdbf.free_result(_tps_db_handle, db_res)<0)
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

#define TPS_DB_ADD_STRV(dcol, dval, cnr, cname, cval) \
	do { \
		if(cval.len>0) { \
			dcol[cnr] = &cname; \
			dval[cnr].type = DB1_STR; \
			dval[cnr].val.str_val = TPS_STRZ(cval); \
			cnr++; \
		} \
	} while(0)

/**
 *
 */
int tps_db_update_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode)
{
	db_key_t db_keys[8];
	db_op_t  db_ops[8];
	db_val_t db_vals[8];
	db_key_t db_ucols[TPS_NR_KEYS];
	db_val_t db_uvals[TPS_NR_KEYS];
	int nr_keys;
	int nr_ucols;

	if(_tps_db_handle==NULL)
		return -1;

	memset(db_ucols, 0, TPS_NR_KEYS*sizeof(db_key_t));
	memset(db_uvals, 0, TPS_NR_KEYS*sizeof(db_val_t));

	nr_keys = 0;
	nr_ucols = 0;

	db_keys[nr_keys]=&td_col_a_uuid;
	db_ops[nr_keys]=OP_EQ;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	if(sd->a_uuid.len>0 && sd->a_uuid.s[0]=='a') {
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->a_uuid);
	} else {
		if(sd->b_uuid.len<=0) {
			LM_ERR("no valid dlg uuid\n");
			return -1;
		}
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->b_uuid);
	}
	nr_keys++;

	if(mode & TPS_DBU_CONTACT) {
		TPS_DB_ADD_STRV(db_ucols, db_uvals, nr_ucols,
				tt_col_a_contact, md->a_contact);
		TPS_DB_ADD_STRV(db_ucols, db_uvals, nr_ucols,
				tt_col_b_contact, md->b_contact);
	}

	if((mode & TPS_DBU_RPLATTRS) && msg->first_line.type==SIP_REPLY) {
		if(sd->b_tag.len<=0
				&& msg->first_line.u.reply.statuscode>=180
				&& msg->first_line.u.reply.statuscode<200) {

			db_ucols[nr_ucols] = &tt_col_y_rr;
			db_uvals[nr_ucols].type = DB1_STR;
			db_uvals[nr_ucols].val.str_val = TPS_STRZ(md->b_rr);
			nr_ucols++;

			TPS_DB_ADD_STRV(
					db_ucols, db_uvals, nr_ucols, tt_col_b_tag, md->b_tag);
		}
	}
	if(nr_ucols==0) {
		return 0;
	}
	if (_tpsdbf.use_table(_tps_db_handle, &tt_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.update(_tps_db_handle, db_keys, db_ops, db_vals,
				db_ucols, db_uvals, nr_keys, nr_ucols)!=0) {
		LM_ERR("failed to do branch db update for [%.*s]!\n",
				md->a_uuid.len, md->a_uuid.s);
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_storage_update_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode)
{
	int ret;

	if(msg==NULL || md==NULL || sd==NULL)
		return -1;

	if(md->s_method_id != METHOD_INVITE) {
		return 0;
	}

	if(msg->first_line.type==SIP_REPLY) {
		if(msg->first_line.u.reply.statuscode < 180
				|| msg->first_line.u.reply.statuscode >= 200) {
			return 0;
		}
	}

	ret = tps_storage_link_msg(msg, md, md->direction);
	if(ret<0) return -1;

	return _tps_storage_api.update_branch(msg, md, sd, mode);
}

/**
 *
 */
int tps_db_update_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode)
{
	db_key_t db_keys[8];
	db_op_t  db_ops[8];
	db_val_t db_vals[8];
	db_key_t db_ucols[TPS_NR_KEYS];
	db_val_t db_uvals[TPS_NR_KEYS];
	int nr_keys;
	int nr_ucols;

	if(_tps_db_handle==NULL)
		return -1;

	memset(db_ucols, 0, TPS_NR_KEYS*sizeof(db_key_t));
	memset(db_uvals, 0, TPS_NR_KEYS*sizeof(db_val_t));

	nr_keys = 0;
	nr_ucols = 0;

	db_keys[nr_keys]=&td_col_a_uuid;
	db_ops[nr_keys]=OP_EQ;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	if(sd->a_uuid.len>0 && sd->a_uuid.s[0]=='a') {
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->a_uuid);
	} else {
		if(sd->b_uuid.len<=0) {
			LM_ERR("no valid dlg uuid (%d:%.*s - %d:%.*s)\n",
					sd->a_uuid.len, sd->a_uuid.len, ZSW(sd->a_uuid.s),
					sd->b_uuid.len, sd->b_uuid.len, ZSW(sd->b_uuid.s));
			return -1;
		}
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->b_uuid);
	}
	nr_keys++;

	if(mode & TPS_DBU_CONTACT) {
		TPS_DB_ADD_STRV(db_ucols, db_uvals, nr_ucols,
				td_col_a_contact, md->a_contact);
		TPS_DB_ADD_STRV(db_ucols, db_uvals, nr_ucols,
				td_col_b_contact, md->b_contact);
	}

	if((mode & TPS_DBU_RPLATTRS) && msg->first_line.type==SIP_REPLY) {
		if(sd->b_tag.len<=0
				&& msg->first_line.u.reply.statuscode>=200
				&& msg->first_line.u.reply.statuscode<300) {

			if((sd->iflags&TPS_IFLAG_DLGON) == 0) {
				db_ucols[nr_ucols] = &td_col_b_rr;
				db_uvals[nr_ucols].type = DB1_STR;
				db_uvals[nr_ucols].val.str_val = TPS_STRZ(md->b_rr);
				nr_ucols++;
			}

			TPS_DB_ADD_STRV(db_ucols, db_uvals, nr_ucols,
					td_col_b_tag, md->b_tag);

			db_ucols[nr_ucols] = &td_col_iflags;
			db_uvals[nr_ucols].type = DB1_INT;
			db_uvals[nr_ucols].val.int_val = sd->iflags|TPS_IFLAG_DLGON;
			nr_ucols++;
		}
	}
	if(nr_ucols==0) {
		return 0;
	}
	if (_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.update(_tps_db_handle, db_keys, db_ops, db_vals,
				db_ucols, db_uvals, nr_keys, nr_ucols)!=0) {
		LM_ERR("failed to do dialog db update for [%.*s]!\n",
				md->a_uuid.len, md->a_uuid.s);
		return -1;
	}
	return 0;
}

/**
 *
 */
int tps_storage_update_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode)
{
	int ret;

	if(msg==NULL || md==NULL || sd==NULL)
		return -1;

	if(md->s_method_id != METHOD_INVITE) {
		return 0;
	}
	if(msg->first_line.type==SIP_REPLY) {
		if(msg->first_line.u.reply.statuscode < 200
				|| msg->first_line.u.reply.statuscode >= 300) {
			return 0;
		}
	}

	ret = tps_storage_link_msg(msg, md, md->direction);
	if(ret<0) return -1;

	return _tps_storage_api.update_dialog(msg, md, sd, mode);
}

/**
 *
 */
int tps_db_end_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	db_key_t db_keys[4];
	db_op_t  db_ops[4];
	db_val_t db_vals[4];
	db_key_t db_ucols[TPS_NR_KEYS];
	db_val_t db_uvals[TPS_NR_KEYS];
	int nr_keys;
	int nr_ucols;

	if(msg==NULL || md==NULL || sd==NULL || _tps_db_handle==NULL)
		return -1;

	if(md->s_method_id != METHOD_BYE) {
		return 0;
	}

	memset(db_ucols, 0, TPS_NR_KEYS*sizeof(db_key_t));
	memset(db_uvals, 0, TPS_NR_KEYS*sizeof(db_val_t));

	nr_keys = 0;
	nr_ucols = 0;

	db_keys[nr_keys]=&td_col_a_uuid;
	db_ops[nr_keys]=OP_EQ;
	db_vals[nr_keys].type = DB1_STR;
	db_vals[nr_keys].nul = 0;
	if(sd->a_uuid.len>0 && sd->a_uuid.s[0]=='a') {
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->a_uuid);
	} else {
		if(sd->b_uuid.len<=0) {
			LM_ERR("no valid dlg uuid\n");
			return -1;
		}
		db_vals[nr_keys].val.str_val = TPS_STRZ(sd->b_uuid);
	}
	nr_keys++;

	db_ucols[nr_ucols] = &td_col_rectime;
	db_uvals[nr_ucols].type = DB1_DATETIME;
	db_uvals[nr_ucols].val.time_val = time(NULL);
	nr_ucols++;

	db_ucols[nr_ucols] = &td_col_iflags;
	db_uvals[nr_ucols].type = DB1_INT;
	db_uvals[nr_ucols].val.int_val = 0;
	nr_ucols++;

	if (_tpsdbf.use_table(_tps_db_handle, &td_table_name) < 0) {
		LM_ERR("failed to perform use table\n");
		return -1;
	}

	if(_tpsdbf.update(_tps_db_handle, db_keys, db_ops, db_vals,
				db_ucols, db_uvals, nr_keys, nr_ucols)!=0) {
		LM_ERR("failed to do db update for [%.*s]!\n",
				md->a_uuid.len, md->a_uuid.s);
		return -1;
	}
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
void tps_storage_clean(unsigned int ticks, void* param)
{
	_tps_storage_api.clean_branches();
	_tps_storage_api.clean_dialogs();
}
