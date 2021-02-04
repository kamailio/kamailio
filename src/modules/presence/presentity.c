/*
 * Copyright (C) 2006 Voice Sistem S.R.L.
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

/*!
 * \file
 * \brief Kamailio presence module :: Presentity handling
 * \ingroup presence
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../lib/srdb1/db.h"
#include "../../core/hashes.h"
#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/xavp.h"
#include "../../core/str.h"
#include "../../core/data_lump_rpl.h"
#include "presentity.h"
#include "presence.h"
#include "notify.h"
#include "publish.h"
#include "hash.h"
#include "utils_func.h"
#include "presence_dmq.h"


/* base priority value (20150101T000000) */
#define PRES_PRIORITY_TBASE 1420070400

xmlNodePtr xmlNodeGetNodeByName(
		xmlNodePtr node, const char *name, const char *ns);
static str pu_200_rpl = str_init("OK");
static str pu_412_rpl = str_init("Conditional request failed");

static str str_offline_etag_val = str_init("*#-OFFLINE-#*");

#define ETAG_LEN 128

char *generate_ETag(int publ_count)
{
	char *etag = NULL;
	int size = 0;

	etag = (char *)pkg_malloc(ETAG_LEN * sizeof(char));
	if(etag == NULL) {
		ERR_MEM(PKG_MEM_STR);
	}
	memset(etag, 0, ETAG_LEN * sizeof(char));
	size = snprintf(etag, ETAG_LEN, "%c.%d.%d.%d.%d", pres_prefix,
			pres_startup_time, pres_pid, pres_counter, publ_count);
	if(size < 0) {
		LM_ERR("unsuccessful snprintf\n ");
		pkg_free(etag);
		return NULL;
	}
	if(size >= ETAG_LEN) {
		LM_ERR("buffer size overflown\n");
		pkg_free(etag);
		return NULL;
	}

	etag[size] = '\0';
	LM_DBG("etag= %s / %d\n ", etag, size);
	return etag;

error:
	return NULL;
}

int publ_send200ok(struct sip_msg *msg, int lexpire, str etag)
{
	char buf[128];
	int buf_len = 128, size;
	str hdr_append = {0, 0}, hdr_append2 = {0, 0};

	if(msg == NULL)
		return 0;

	LM_DBG("send 200OK reply\n");
	LM_DBG("etag= %s - len= %d\n", etag.s, etag.len);

	hdr_append.s = buf;
	hdr_append.s[0] = '\0';
	hdr_append.len = snprintf(hdr_append.s, buf_len, "Expires: %d\r\n",
			((lexpire == 0) ? 0 : (lexpire - pres_expires_offset)));
	if(hdr_append.len < 0) {
		LM_ERR("unsuccessful snprintf\n");
		goto error;
	}
	if(hdr_append.len >= buf_len) {
		LM_ERR("buffer size overflown\n");
		goto error;
	}
	hdr_append.s[hdr_append.len] = '\0';

	if(add_lump_rpl(msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR) == 0) {
		LM_ERR("unable to add lump_rl\n");
		goto error;
	}

	size = sizeof(char) * (20 + etag.len);
	hdr_append2.s = (char *)pkg_malloc(size);
	if(hdr_append2.s == NULL) {
		ERR_MEM(PKG_MEM_STR);
	}
	hdr_append2.s[0] = '\0';
	hdr_append2.len = snprintf(hdr_append2.s, size, "SIP-ETag: %s\r\n", etag.s);
	if(hdr_append2.len < 0) {
		LM_ERR("unsuccessful snprintf\n ");
		goto error;
	}
	if(hdr_append2.len >= size) {
		LM_ERR("buffer size overflown\n");
		goto error;
	}

	hdr_append2.s[hdr_append2.len] = '\0';
	if(add_lump_rpl(msg, hdr_append2.s, hdr_append2.len, LUMP_RPL_HDR) == 0) {
		LM_ERR("unable to add lump_rl\n");
		goto error;
	}

	if(slb.freply(msg, 200, &pu_200_rpl) < 0) {
		LM_ERR("sending reply\n");
		goto error;
	}

	pkg_free(hdr_append2.s);
	return 0;

error:

	if(hdr_append2.s)
		pkg_free(hdr_append2.s);

	return -1;
}

/**
 * get priority value for presence document
 */
unsigned int pres_get_priority(void)
{
	sr_xavp_t *vavp = NULL;
	str vname = str_init("priority");

	if(pres_xavp_cfg.s == NULL || pres_xavp_cfg.len <= 0) {
		return 0;
	}

	vavp = xavp_get_child_with_ival(&pres_xavp_cfg, &vname);
	if(vavp != NULL) {
		return (unsigned int)vavp->val.v.i;
	}

	return (unsigned int)(time(NULL) - PRES_PRIORITY_TBASE);
}

/**
 * create new presentity record
 */
presentity_t *new_presentity(str *domain, str *user, int expires,
		pres_ev_t *event, str *etag, str *sender)
{
	presentity_t *presentity = NULL;
	int size, init_len;

	/* allocating memory for presentity */
	size = sizeof(presentity_t) + domain->len + user->len + etag->len + 1;
	if(sender)
		size += sizeof(str) + sender->len * sizeof(char);

	init_len = size;

	presentity = (presentity_t *)pkg_malloc(size);
	if(presentity == NULL) {
		ERR_MEM(PKG_MEM_STR);
	}
	memset(presentity, 0, size);
	size = sizeof(presentity_t);

	presentity->domain.s = (char *)presentity + size;
	strncpy(presentity->domain.s, domain->s, domain->len);
	presentity->domain.len = domain->len;
	size += domain->len;

	presentity->user.s = (char *)presentity + size;
	strncpy(presentity->user.s, user->s, user->len);
	presentity->user.len = user->len;
	size += user->len;

	presentity->etag.s = (char *)presentity + size;
	memcpy(presentity->etag.s, etag->s, etag->len);
	presentity->etag.s[etag->len] = '\0';
	presentity->etag.len = etag->len;

	size += etag->len + 1;

	if(sender) {
		presentity->sender = (str *)((char *)presentity + size);
		size += sizeof(str);
		presentity->sender->s = (char *)presentity + size;
		memcpy(presentity->sender->s, sender->s, sender->len);
		presentity->sender->len = sender->len;
		size += sender->len;
	}

	if(size > init_len) {
		LM_ERR("buffer size overflow init_len= %d, size= %d\n", init_len, size);
		goto error;
	}
	presentity->event = event;
	presentity->expires = expires;
	presentity->received_time = (int)time(NULL);
	presentity->priority = pres_get_priority();
	return presentity;

error:
	if(presentity)
		pkg_free(presentity);
	return NULL;
}

xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name)
{
	xmlNodePtr cur = node->children;
	while(cur) {
		if(xmlStrcasecmp(cur->name, (unsigned char *)name) == 0)
			return cur;
		cur = cur->next;
	}
	return NULL;
}

int check_if_dialog(str body, int *is_dialog, char **dialog_id)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *tmp_dialog_id;

	*dialog_id = NULL;
	*is_dialog = 0;

	doc = xmlParseMemory(body.s, body.len);
	if(doc == NULL) {
		LM_INFO("failed to parse xml document\n");
		return -1;
	}

	node = doc->children;
	node = xmlNodeGetChildByName(node, "dialog");

	if(node != NULL) {
		*is_dialog = 1;
		tmp_dialog_id = (char *)xmlGetProp(node, (xmlChar *)"id");

		if(tmp_dialog_id != NULL) {
			*dialog_id = strdup(tmp_dialog_id);
			xmlFree(tmp_dialog_id);
		}
	}

	xmlFreeDoc(doc);
	return 0;
}

/**
 * return 1 if all dialog nodes have the vstate
 *   * 0 if at least one state is different
 *   * -1 in case of error
 */
int ps_match_dialog_state_from_body(str body, int *is_dialog, char *vstate)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	xmlNodePtr fnode;
	xmlNodePtr childNode;
	char *tmp_state;
	int rmatch = 0;

	*is_dialog = 0;

	doc = xmlParseMemory(body.s, body.len);
	if(doc == NULL || doc->children == NULL) {
		LM_ERR("failed to parse xml document\n");
		return -1;
	}

	fnode = node = xmlNodeGetChildByName(doc->children, "dialog");

	while(node != NULL) {
		*is_dialog = 1;

		childNode = xmlNodeGetChildByName(node, "state");
		tmp_state = (char *)xmlNodeGetContent(childNode);

		if(tmp_state != NULL) {
			if(strcmp(tmp_state, vstate) != 0) {
				/* state not matched */
				xmlFree(tmp_state);
				rmatch = 0;
				goto done;
			}
			rmatch = 1;
			xmlFree(tmp_state);
		}
		/* search for next dialog node */
		do {
			if(node->next != NULL && node->next->name != NULL
					&& xmlStrcmp(fnode->name, node->next->name) == 0) {
				node = node->next;
				break;
			}
			node = node->next;
		} while(node != NULL);
	}

done:
	xmlFreeDoc(doc);
	return rmatch;
}

int ps_db_delete_presentity_if_dialog_id_exists(
		presentity_t *presentity, char *dialog_id)
{
	db_key_t query_cols[13], result_cols[6];
	db_op_t query_ops[13];
	db_val_t query_vals[13];
	int n_query_cols = 0;
	int rez_body_col = 0, rez_etag_col = 0, n_result_cols = 0;
	db1_res_t *result = NULL;
	db_row_t *row = NULL;
	db_val_t *row_vals = NULL;
	char *db_dialog_id = NULL;
	int db_is_dialog = 0;
	str tmp_db_body, tmp_db_etag;
	int i = 0;
	presentity_t old_presentity;

	if(presentity->event->evp->type != EVENT_DIALOG) {
		return 0;
	}

	query_cols[n_query_cols] = &str_domain_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->domain;
	n_query_cols++;

	query_cols[n_query_cols] = &str_username_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_event_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->event->name;
	n_query_cols++;

	result_cols[rez_body_col = n_result_cols++] = &str_body_col;
	result_cols[rez_etag_col = n_result_cols++] = &str_etag_col;

	if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
		LM_ERR("unsuccessful sql use table\n");
		return -1;
	}

	if(pa_dbf.query(pa_db, query_cols, query_ops, query_vals, result_cols,
			   n_query_cols, n_result_cols, 0, &result)
			< 0) {
		LM_ERR("unsuccessful sql query\n");
		return -2;
	}

	if(result == NULL)
		return -3;

	/* no results from query definitely means no dialog exists */
	if(result->n <= 0) {
		pa_dbf.free_result(pa_db, result);
		return 0;
	}

	// Loop the rows returned from the DB
	for(i = 0; i < result->n; i++) {
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);
		tmp_db_body.s = (char *)row_vals[rez_body_col].val.string_val;
		tmp_db_body.len = strlen(tmp_db_body.s);

		tmp_db_etag.s = (char *)row_vals[rez_etag_col].val.string_val;
		tmp_db_etag.len = strlen(tmp_db_etag.s);

		if(check_if_dialog(tmp_db_body, &db_is_dialog, &db_dialog_id) == 0) {
			// If ID from DB matches the one we supplied
			if(db_dialog_id && !strcmp(db_dialog_id, dialog_id)) {
				old_presentity.domain = presentity->domain;
				old_presentity.user = presentity->user;
				old_presentity.event = presentity->event;
				old_presentity.etag = tmp_db_etag;

				LM_DBG("Presentity found - deleting it\n");

				if(delete_presentity(&old_presentity, NULL) < 0) {
					LM_ERR("failed to delete presentity\n");
				}

				pa_dbf.free_result(pa_db, result);
				result = NULL;
				free(db_dialog_id);
				db_dialog_id = NULL;

				return 1;
			}

			free(db_dialog_id);
			db_dialog_id = NULL;
		}
	}

	pa_dbf.free_result(pa_db, result);
	result = NULL;
	return 0;
}

int ps_cache_delete_presentity_if_dialog_id_exists(
		presentity_t *presentity, char *dialog_id)
{
	char *db_dialog_id = NULL;
	int db_is_dialog = 0;
	presentity_t old_presentity;
	ps_presentity_t ptm;
	ps_presentity_t *ptx = NULL;
	ps_presentity_t *ptlist = NULL;

	if(presentity->event->evp->type != EVENT_DIALOG) {
		return 0;
	}
	memset(&ptm, 0, sizeof(ps_presentity_t));

	ptm.user = presentity->user;
	ptm.domain = presentity->domain;
	ptm.event = presentity->event->name;

	ptlist = ps_ptable_search(&ptm, 1, 0);

	if(ptlist == NULL) {
		return 0;
	}

	for(ptx=ptlist; ptx!=NULL; ptx=ptx->next) {
		if(check_if_dialog(ptx->body, &db_is_dialog, &db_dialog_id) == 0) {
			// If ID from DB matches the one we supplied
			if(db_dialog_id && !strcmp(db_dialog_id, dialog_id)) {
				old_presentity.domain = presentity->domain;
				old_presentity.user = presentity->user;
				old_presentity.event = presentity->event;
				old_presentity.etag = ptx->etag;

				LM_DBG("Presentity found - deleting it\n");

				if(delete_presentity(&old_presentity, NULL) < 0) {
					LM_ERR("failed to delete presentity\n");
				}
				ps_presentity_list_free(ptlist, 1);
				free(db_dialog_id);
				db_dialog_id = NULL;
				return 1;
			}
			free(db_dialog_id);
			db_dialog_id = NULL;
		}
	}
	ps_presentity_list_free(ptlist, 1);
	return 0;
}

int delete_presentity_if_dialog_id_exists(
		presentity_t *presentity, char *dialog_id)
{
	if(publ_cache_mode == PS_PCACHE_RECORD) {
		return ps_cache_delete_presentity_if_dialog_id_exists(presentity,
				dialog_id);
	} else {
		return ps_db_delete_presentity_if_dialog_id_exists(presentity,
				dialog_id);
	}
}

/**
 *
 */
int ps_db_match_dialog_state(presentity_t *presentity, char *vstate)
{
	db_key_t query_cols[13], result_cols[6];
	db_op_t query_ops[13];
	db_val_t query_vals[13];
	int n_query_cols = 0;
	int rez_body_col = 0, n_result_cols = 0;
	db1_res_t *result = NULL;
	db_row_t *row = NULL;
	db_val_t *row_vals = NULL;
	int db_is_dialog = 0;
	str tmp_db_body;
	int i = 0, rmatch = 0;

	query_cols[n_query_cols] = &str_domain_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->domain;
	n_query_cols++;

	query_cols[n_query_cols] = &str_username_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_event_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->event->name;
	n_query_cols++;

	query_cols[n_query_cols] = &str_etag_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->etag;
	n_query_cols++;

	result_cols[rez_body_col = n_result_cols++] = &str_body_col;

	if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
		LM_ERR("unsuccessful sql use table\n");
		return -1;
	}

	if(pa_dbf.query(pa_db, query_cols, query_ops, query_vals, result_cols,
			   n_query_cols, n_result_cols, 0, &result)
			< 0) {
		LM_ERR("unsuccessful sql query\n");
		return -2;
	}

	if(result == NULL)
		return -3;

	/* no results from query definitely means no dialog exists */
	if(result->n <= 0) {
		pa_dbf.free_result(pa_db, result);
		return 0;
	}

	rmatch = 0;
	// Loop the rows returned from the DB
	for(i = 0; i < result->n; i++) {
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);
		tmp_db_body.s = (char *)row_vals[rez_body_col].val.string_val;
		tmp_db_body.len = strlen(tmp_db_body.s);

		rmatch = ps_match_dialog_state_from_body(
				tmp_db_body, &db_is_dialog, vstate);

		if(rmatch == 1) {
			/* having a full match */
			pa_dbf.free_result(pa_db, result);
			result = NULL;
			return rmatch;
		}
	}

	pa_dbf.free_result(pa_db, result);
	result = NULL;
	return rmatch;
}

/**
 *
 */
int ps_cache_match_dialog_state(presentity_t *presentity, char *vstate)
{
	int db_is_dialog = 0;
	int rmatch = 0;
	ps_presentity_t *ptx = NULL;
	ps_presentity_t *ptlist = NULL;
	ps_presentity_t ptm;

	memset(&ptm, 0, sizeof(ps_presentity_t));

	ptm.user = presentity->user;
	ptm.domain = presentity->domain;
	ptm.event = presentity->event->name;
	ptm.etag = presentity->etag;

	ptlist = ps_ptable_search(&ptm, 2, 0);

	if(ptlist==NULL) {
		return 0;
	}

	for(ptx=ptlist; ptx!=NULL; ptx=ptx->next) {
		rmatch = ps_match_dialog_state_from_body(ptx->body, &db_is_dialog, vstate);

		if(rmatch == 1) {
			/* having a full match */
			ps_presentity_list_free(ptlist, 1);
			return rmatch;
		}
	}

	ps_presentity_list_free(ptlist, 1);
	return rmatch;
}

/**
 * check if the states of all related dialogs match vstate
 */
int ps_match_dialog_state(presentity_t *presentity, char *vstate)
{
	if(publ_cache_mode == PS_PCACHE_RECORD) {
		return ps_cache_match_dialog_state(presentity, vstate);
	} else {
		return ps_db_match_dialog_state(presentity, vstate);
	}
}

static int ps_db_update_presentity(sip_msg_t *msg, presentity_t *presentity,
		str *body, int new_t, int *sent_reply, char *sphere, str *etag_override,
		str *ruid, int replace)
{
	db_key_t query_cols[14], rquery_cols[2], update_keys[9], result_cols[7];
	db_op_t query_ops[14], rquery_ops[2];
	db_val_t query_vals[14], rquery_vals[2], update_vals[9];
	db1_res_t *result = NULL;
	int n_query_cols = 0;
	int n_rquery_cols = 0;
	int n_update_cols = 0;
	char *dot = NULL;
	str etag = {0, 0};
	str cur_etag = {0, 0};
	str *rules_doc = NULL;
	str pres_uri = {0, 0};
	int rez_body_col, rez_sender_col, rez_ruid_col, n_result_cols = 0;
	db_row_t *row = NULL;
	db_val_t *row_vals = NULL;
	str old_body, sender;
	int is_dialog = 0, bla_update_publish = 1;
	int affected_rows = 0;
	int ret = -1;
	int db_record_exists = 0;
	int num_watchers = 0;
	char *old_dialog_id = NULL, *dialog_id = NULL;
	str cur_ruid = {0, 0};
	str p_ruid = {0, 0};

	if(sent_reply)
		*sent_reply = 0;
	if(pres_notifier_processes == 0 && presentity->event->req_auth) {
		/* get rules_document */
		if(presentity->event->get_rules_doc(
				   &presentity->user, &presentity->domain, &rules_doc)
				< 0) {
			LM_ERR("getting rules doc\n");
			goto error;
		}
	}

	if(uandd_to_uri(presentity->user, presentity->domain, &pres_uri) < 0) {
		LM_ERR("constructing uri from user and domain\n");
		goto error;
	}


	query_cols[n_query_cols] = &str_domain_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->domain;
	n_query_cols++;

	query_cols[n_query_cols] = &str_username_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_event_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->event->name;
	n_query_cols++;

	query_cols[n_query_cols] = &str_etag_col;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = presentity->etag;
	n_query_cols++;

	result_cols[rez_body_col = n_result_cols++] = &str_body_col;
	result_cols[rez_sender_col = n_result_cols++] = &str_sender_col;
	result_cols[rez_ruid_col = n_result_cols++] = &str_ruid_col;

	if(new_t) {
		LM_DBG("new presentity with etag %.*s\n", presentity->etag.len,
				presentity->etag.s);

		if(ruid) {
			/* use the provided ruid */
			p_ruid = *ruid;
		} else {
			/* generate a new ruid */
			if(sruid_next(&pres_sruid) < 0)
				goto error;
			p_ruid = pres_sruid.uid;
		}

		/* insert new record in hash_table */

		if(publ_cache_mode==PS_PCACHE_HYBRID
				&& insert_phtable(
						   &pres_uri, presentity->event->evp->type, sphere)
						   < 0) {
			LM_ERR("inserting record in hash table\n");
			goto error;
		}

		LM_DBG("new htable record added\n");
		if(presentity->expires == -1) {
			replace = 1;
		}

		/* insert new record into database */
		query_cols[n_query_cols] = &str_sender_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		if(presentity->sender) {
			query_vals[n_query_cols].val.str_val.s = presentity->sender->s;
			query_vals[n_query_cols].val.str_val.len = presentity->sender->len;
		} else {
			query_vals[n_query_cols].val.str_val.s = "";
			query_vals[n_query_cols].val.str_val.len = 0;
		}
		n_query_cols++;

		query_cols[n_query_cols] = &str_body_col;
		query_vals[n_query_cols].type = DB1_BLOB;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = *body;
		n_query_cols++;

		query_cols[n_query_cols] = &str_received_time_col;
		query_vals[n_query_cols].type = DB1_INT;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.int_val = presentity->received_time;
		n_query_cols++;

		query_cols[n_query_cols] = &str_priority_col;
		query_vals[n_query_cols].type = DB1_INT;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.int_val = presentity->priority;
		n_query_cols++;

		query_cols[n_query_cols] = &str_ruid_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = p_ruid;
		n_query_cols++;

		if(!replace) {
			/* A real PUBLISH */
			query_cols[n_query_cols] = &str_expires_col;
			query_vals[n_query_cols].type = DB1_INT;
			query_vals[n_query_cols].nul = 0;
			query_vals[n_query_cols].val.int_val =
					presentity->expires + (int)time(NULL);
			n_query_cols++;

			if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
				LM_ERR("unsuccessful use_table\n");
				goto error;
			}

			if(pa_dbf.start_transaction) {
				if(pa_dbf.start_transaction(pa_db, pres_db_table_lock) < 0) {
					LM_ERR("in start_transaction\n");
					goto error;
				}
			}
			if(presentity->event->evp->type == EVENT_DIALOG) {
				check_if_dialog(*body, &is_dialog, &dialog_id);
				if(dialog_id) {
					if(delete_presentity_if_dialog_id_exists(
							   presentity, dialog_id)
							< 0) {
						free(dialog_id);
						dialog_id = NULL;
						goto error;
					}

					free(dialog_id);
					dialog_id = NULL;
				}
			}
			LM_DBG("inserting %d cols into table\n", n_query_cols);

			if(pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0) {
				LM_ERR("inserting new record in database\n");
				goto error;
			}
		} else {
			/* A hard-state PUBLISH */
			query_cols[n_query_cols] = &str_expires_col;
			query_vals[n_query_cols].type = DB1_INT;
			query_vals[n_query_cols].nul = 0;
			query_vals[n_query_cols].val.int_val = -1;
			if(presentity->expires != -1) {
				query_vals[n_query_cols].val.int_val =
						presentity->expires + (int)time(NULL);
			}
			n_query_cols++;

			if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
				LM_ERR("unsuccessful use_table\n");
				goto error;
			}

			if(pa_dbf.replace == NULL) {
				LM_ERR("replace is required for pidf-manipulation support\n");
				goto error;
			}

			if(pa_dbf.start_transaction) {
				if(pa_dbf.start_transaction(pa_db, pres_db_table_lock) < 0) {
					LM_ERR("in start_transaction\n");
					goto error;
				}
			}


			if(pa_dbf.replace(pa_db, query_cols, query_vals, n_query_cols, 4, 0)
					< 0) {
				LM_ERR("replacing record in database\n");
				goto error;
			}
		}

		if(publ_send200ok(msg, presentity->expires, presentity->etag) < 0) {
			LM_ERR("sending 200OK\n");
			goto error;
		}
		if(sent_reply)
			*sent_reply = 1;
		goto send_notify;
	} else {
		LM_DBG("updating existing presentity with etag %.*s\n",
				presentity->etag.len, presentity->etag.s);

		if(ruid) {
			p_ruid = *ruid;

			rquery_cols[n_rquery_cols] = &str_ruid_col;
			rquery_ops[n_rquery_cols] = OP_EQ;
			rquery_vals[n_rquery_cols].type = DB1_STR;
			rquery_vals[n_rquery_cols].nul = 0;
			rquery_vals[n_rquery_cols].val.str_val = p_ruid;
			n_rquery_cols++;

			// TODO: check for out-of-sequence updates
		}

		if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
			LM_ERR("unsuccessful sql use table\n");
			goto error;
		}

		if(pa_dbf.start_transaction) {
			if(pa_dbf.start_transaction(pa_db, pres_db_table_lock) < 0) {
				LM_ERR("in start_transaction\n");
				goto error;
			}
		}

		if(EVENT_DIALOG_SLA(presentity->event->evp)) {

			if(pa_dbf.query(pa_db, ruid ? rquery_cols : query_cols,
					   ruid ? rquery_ops : query_ops,
					   ruid ? rquery_vals : query_vals, result_cols,
					   ruid ? n_rquery_cols : n_query_cols, n_result_cols, 0,
					   &result)
					< 0) {
				LM_ERR("unsuccessful sql query\n");
				goto error;
			}
			if(result == NULL)
				goto error;

			if(!(result->n > 0))
				goto send_412;

			db_record_exists = 1;
			/* analize if previous body has a dialog */
			row = &result->rows[0];
			row_vals = ROW_VALUES(row);

			/* store current ruid if we don't already know it */
			if(!p_ruid.s && row_vals[rez_ruid_col].val.string_val) {
				cur_ruid.len =
						strlen((char *)row_vals[rez_ruid_col].val.string_val);
				cur_ruid.s = (char *)pkg_malloc(sizeof(char) * cur_ruid.len);
				if(!cur_ruid.s) {
					LM_ERR("no private memory\n");
					goto error;
				}
				memcpy(cur_ruid.s,
						(char *)row_vals[rez_ruid_col].val.string_val,
						cur_ruid.len);
				p_ruid = cur_ruid;
			}

			old_body.s = (char *)row_vals[rez_body_col].val.string_val;
			old_body.len = strlen(old_body.s);
			if(check_if_dialog(*body, &is_dialog, &dialog_id) < 0) {
				LM_ERR("failed to check if dialog stored\n");
				if(dialog_id) {
					free(dialog_id);
					dialog_id = NULL;
				}
				goto error;
			}

			free(dialog_id);

			if(is_dialog == 1) /* if the new body has a dialog - overwrite */
			{
				goto after_dialog_check;
			}

			if(check_if_dialog(old_body, &is_dialog, &old_dialog_id) < 0) {
				LM_ERR("failed to check if dialog stored\n");
				if(old_dialog_id) {
					free(old_dialog_id);
					old_dialog_id = NULL;
				}
				goto error;
			}

			/* if the old body has no dialog - overwrite */
			if(is_dialog == 0) {
				if(old_dialog_id) {
					free(old_dialog_id);
					old_dialog_id = NULL;
				}
				goto after_dialog_check;
			}

			if(old_dialog_id) {
				free(old_dialog_id);
				old_dialog_id = NULL;
			}

			sender.s = (char *)row_vals[rez_sender_col].val.string_val;
			sender.len = strlen(sender.s);

			LM_DBG("old_sender = %.*s\n", sender.len, sender.s);
			if(presentity->sender) {
				if(!(presentity->sender->len == sender.len
						   && presence_sip_uri_match(
									  presentity->sender, &sender)
									  == 0))
					bla_update_publish = 0;
			}
		after_dialog_check:
			pa_dbf.free_result(pa_db, result);
			result = NULL;
		}

		if(presentity->expires <= 0) {

			if(!db_record_exists) {
				if(pa_dbf.query(pa_db, ruid ? rquery_cols : query_cols,
						   ruid ? rquery_ops : query_ops,
						   ruid ? rquery_vals : query_vals, result_cols,
						   ruid ? n_rquery_cols : n_query_cols, n_result_cols,
						   0, &result)
						< 0) {
					LM_ERR("unsuccessful sql query\n");
					goto error;
				}
				if(result == NULL)
					goto error;

				if(!(result->n > 0))
					goto send_412;

				db_record_exists = 1;

				row = &result->rows[0];
				row_vals = ROW_VALUES(row);

				/* store current ruid if we don't already know it */
				if(!p_ruid.s && row_vals[rez_ruid_col].val.string_val) {
					cur_ruid.len = strlen(
							(char *)row_vals[rez_ruid_col].val.string_val);
					cur_ruid.s =
							(char *)pkg_malloc(sizeof(char) * cur_ruid.len);
					if(!cur_ruid.s) {
						LM_ERR("no private memory\n");
						goto error;
					}
					memcpy(cur_ruid.s,
							(char *)row_vals[rez_ruid_col].val.string_val,
							cur_ruid.len);
					p_ruid = cur_ruid;
				}

				pa_dbf.free_result(pa_db, result);
				result = NULL;
			}

			if(publ_send200ok(msg, presentity->expires, presentity->etag) < 0) {
				LM_ERR("sending 200OK reply\n");
				goto error;
			}
			if(sent_reply)
				*sent_reply = 1;

			if(pres_notifier_processes > 0) {
				if((num_watchers = publ_notify_notifier(
							pres_uri, presentity->event))
						< 0) {
					LM_ERR("updating watcher records\n");
					goto error;
				}

				if(num_watchers > 0) {
					if(mark_presentity_for_delete(presentity, &p_ruid) < 0) {
						LM_ERR("Marking presentities\n");
						goto error;
					}
				}
			} else {
				if(publ_notify(presentity, pres_uri, body, &presentity->etag,
						   rules_doc)
						< 0) {
					LM_ERR("while sending notify\n");
					goto error;
				}
			}

			if(pres_notifier_processes == 0 || num_watchers == 0) {
				if(delete_presentity(presentity, &p_ruid) < 0) {
					LM_ERR("Deleting presentity\n");
					goto error;
				}

				LM_DBG("deleted from db %.*s\n", presentity->user.len,
						presentity->user.s);
			}

			/* delete from hash table */
			if(publ_cache_mode == PS_PCACHE_HYBRID
					&& delete_phtable(&pres_uri, presentity->event->evp->type)
							   < 0) {
				LM_ERR("deleting record from hash table\n");
				goto error;
			}
			goto done;
		}

		n_update_cols = 0;
		/* if event dialog and is_dialog -> if sender not the same as
		 * old sender do not overwrite */
		if(EVENT_DIALOG_SLA(presentity->event->evp)
				&& bla_update_publish == 0) {
			LM_DBG("drop Publish for BLA from a different sender that"
				   " wants to overwrite an existing dialog\n");
			LM_DBG("sender = %.*s\n", presentity->sender->len,
					presentity->sender->s);
			if(publ_send200ok(msg, presentity->expires, presentity->etag) < 0) {
				LM_ERR("sending 200OK reply\n");
				goto error;
			}
			if(sent_reply)
				*sent_reply = 1;
			goto done;
		}

		if(presentity->event->etag_not_new == 0 || etag_override) {
			if(etag_override) {
				/* use the supplied etag */
				LM_DBG("updating with supplied etag %.*s\n", etag_override->len,
						etag_override->s);
				cur_etag = *etag_override;
				goto after_etag_generation;
			}

			/* generate another etag */
			unsigned int publ_nr;
			str str_publ_nr = {0, 0};

			dot = presentity->etag.s + presentity->etag.len;
			while(*dot != '.' && str_publ_nr.len < presentity->etag.len) {
				str_publ_nr.len++;
				dot--;
			}
			if(str_publ_nr.len == presentity->etag.len) {
				LM_ERR("wrong etag\n");
				goto error;
			}
			str_publ_nr.s = dot + 1;
			str_publ_nr.len--;

			if(str2int(&str_publ_nr, &publ_nr) < 0) {
				LM_ERR("converting string to int\n");
				goto error;
			}
			etag.s = generate_ETag(publ_nr + 1);
			if(etag.s == NULL) {
				LM_ERR("while generating etag\n");
				goto error;
			}
			etag.len = (strlen(etag.s));

			cur_etag = etag;

		after_etag_generation:

			update_keys[n_update_cols] = &str_etag_col;
			update_vals[n_update_cols].type = DB1_STR;
			update_vals[n_update_cols].nul = 0;
			update_vals[n_update_cols].val.str_val = cur_etag;
			n_update_cols++;

		} else
			cur_etag = presentity->etag;

		if(presentity->event->evp->type == EVENT_DIALOG) {
			if(ps_match_dialog_state(presentity, "terminated") == 1) {
				LM_WARN("Trying to update an already terminated state."
						" Skipping update.\n");

				/* send 200OK */
				if(publ_send200ok(msg, presentity->expires, cur_etag) < 0) {
					LM_ERR("sending 200OK reply\n");
					goto error;
				}
				if(sent_reply)
					*sent_reply = 1;

				goto done;
			}
		}

		update_keys[n_update_cols] = &str_expires_col;
		update_vals[n_update_cols].type = DB1_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val =
				presentity->expires + (int)time(NULL);
		n_update_cols++;

		update_keys[n_update_cols] = &str_received_time_col;
		update_vals[n_update_cols].type = DB1_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = presentity->received_time;
		n_update_cols++;

		update_keys[n_update_cols] = &str_priority_col;
		update_vals[n_update_cols].type = DB1_INT;
		update_vals[n_update_cols].nul = 0;
		update_vals[n_update_cols].val.int_val = presentity->priority;
		n_update_cols++;

		if(body && body->s) {
			update_keys[n_update_cols] = &str_body_col;
			update_vals[n_update_cols].type = DB1_BLOB;
			update_vals[n_update_cols].nul = 0;
			update_vals[n_update_cols].val.str_val = *body;
			n_update_cols++;

			/* updated stored sphere */
			if(pres_sphere_enable
					&& presentity->event->evp->type == EVENT_PRESENCE) {
				if(publ_cache_mode == PS_PCACHE_HYBRID
						&& update_phtable(presentity, &pres_uri, body) < 0) {
					LM_ERR("failed to update sphere for presentity\n");
					goto error;
				}
			}
		}

		if(presentity->sender) {
			update_keys[n_update_cols] = &str_sender_col;
			update_vals[n_update_cols].type = DB1_STR;
			update_vals[n_update_cols].nul = 0;
			update_vals[n_update_cols].val.str_val = *presentity->sender;
			n_update_cols++;
		}

		/* if there is no support for affected_rows and no previous query has been done,
		 * or dmq replication is enabled and we don't already know the ruid, do query */
		if((!pa_dbf.affected_rows && !db_record_exists)
				|| (pres_enable_dmq > 0 && !p_ruid.s)) {
			if(pa_dbf.query(pa_db, ruid ? rquery_cols : query_cols,
					   ruid ? rquery_ops : query_ops,
					   ruid ? rquery_vals : query_vals, result_cols,
					   ruid ? n_rquery_cols : n_query_cols, n_result_cols, 0,
					   &result)
					< 0) {
				LM_ERR("unsuccessful sql query\n");
				goto error;
			}
			if(result == NULL)
				goto error;

			if(!(result->n > 0))
				goto send_412;

			db_record_exists = 1;
			affected_rows = result->n;

			row = &result->rows[0];
			row_vals = ROW_VALUES(row);

			/* store current ruid if we don't already know it */
			if(!p_ruid.s && row_vals[rez_ruid_col].val.string_val) {
				cur_ruid.len =
						strlen((char *)row_vals[rez_ruid_col].val.string_val);
				cur_ruid.s = (char *)pkg_malloc(sizeof(char) * cur_ruid.len);
				if(!cur_ruid.s) {
					LM_ERR("no private memory\n");
					goto error;
				}
				memcpy(cur_ruid.s,
						(char *)row_vals[rez_ruid_col].val.string_val,
						cur_ruid.len);
				p_ruid = cur_ruid;

				LM_DBG("existing ruid %.*s\n", p_ruid.len, p_ruid.s);
			}

			pa_dbf.free_result(pa_db, result);
			result = NULL;
		}

		if(pa_dbf.update(pa_db, ruid ? rquery_cols : query_cols,
				   ruid ? rquery_ops : query_ops,
				   ruid ? rquery_vals : query_vals, update_keys, update_vals,
				   ruid ? n_rquery_cols : n_query_cols, n_update_cols)
				< 0) {
			LM_ERR("updating published info in database\n");
			goto error;
		}

		if(pa_dbf.affected_rows && !db_record_exists) {
			if((affected_rows = pa_dbf.affected_rows(pa_db)) < 0) {
				LM_ERR("unsuccessful sql affected rows operation");
				goto error;
			}

			LM_DBG("affected rows after update: %d\n", affected_rows);
		}


		/*if either affected_rows (if exists) or select query show that there is no line in database*/
		if((pa_dbf.affected_rows && !affected_rows && !db_record_exists)
				|| (!pa_dbf.affected_rows && !db_record_exists))
			goto send_412;

		/* send 200OK */
		if(publ_send200ok(msg, presentity->expires, cur_etag) < 0) {
			LM_ERR("sending 200OK reply\n");
			goto error;
		}
		if(sent_reply)
			*sent_reply = 1;

		if(!body)
			goto done;
	}

send_notify:

	/* send notify with presence information */
	if(pres_notifier_processes > 0) {
		if(publ_notify_notifier(pres_uri, presentity->event) < 0) {
			LM_ERR("updating watcher records\n");
			goto error;
		}
	} else {
		if(publ_notify(presentity, pres_uri, body, NULL, rules_doc) < 0) {
			LM_ERR("while sending notify\n");
			goto error;
		}
	}

done:

	if(pres_enable_dmq > 0 && p_ruid.s != NULL) {
		pres_dmq_replicate_presentity(
				presentity, body, new_t, &cur_etag, sphere, &p_ruid, NULL);
	}

	if(etag.s)
		pkg_free(etag.s);
	etag.s = NULL;

	if(cur_ruid.s)
		pkg_free(cur_ruid.s);
	cur_ruid.s = NULL;

	if(rules_doc) {
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
		rules_doc = NULL;
	}
	if(pres_uri.s) {
		pkg_free(pres_uri.s);
		pres_uri.s = NULL;
	}

	if(pa_dbf.end_transaction) {
		if(pa_dbf.end_transaction(pa_db) < 0) {
			LM_ERR("in end_transaction\n");
			goto error;
		}
	}

	return 0;

send_412:

	if(!ruid) {
		LM_ERR("No E_Tag match %*s\n", presentity->etag.len,
				presentity->etag.s);
	} else {
		LM_ERR("No ruid match %*s\n", ruid->len, ruid->s);
	}

	if(msg != NULL) {
		if(slb.freply(msg, 412, &pu_412_rpl) < 0) {
			LM_ERR("sending '412 Conditional request failed' reply\n");
			goto error;
		}
	}
	if(sent_reply)
		*sent_reply = 1;
	ret = 0;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	if(etag.s)
		pkg_free(etag.s);
	if(rules_doc) {
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}
	if(pres_uri.s) {
		pkg_free(pres_uri.s);
	}
	if(cur_ruid.s)
		pkg_free(cur_ruid.s);

	if(pa_dbf.abort_transaction) {
		if(pa_dbf.abort_transaction(pa_db) < 0) {
			LM_ERR("in abort_transaction\n");
		}
	}

	return ret;
}

static int ps_cache_update_presentity(sip_msg_t *msg, presentity_t *presentity,
		str *body, int new_t, int *sent_reply, char *sphere, str *etag_override,
		str *ruid, int replace)
{
	char *dot = NULL;
	str etag = STR_NULL;
	str crt_etag = STR_NULL;
	str *rules_doc = NULL;
	str pres_uri = STR_NULL;
	str old_body = STR_NULL;
	str sender = STR_NULL;
	int is_dialog = 0;
	int bla_update_publish = 1;
	int affected_rows = 0;
	int ret = -1;
	int cache_record_exists = 0;
	int num_watchers = 0;
	char *old_dialog_id = NULL;
	char *dialog_id = NULL;
	str crt_ruid = STR_NULL;
	str p_ruid = STR_NULL;
	ps_presentity_t ptc;
	ps_presentity_t ptm;
	ps_presentity_t *ptx = NULL;

	if(sent_reply) {
		*sent_reply = 0;
	}
	memset(&ptc, 0, sizeof(ps_presentity_t));
	memset(&ptm, 0, sizeof(ps_presentity_t));

	/* here pres_notifier_processes == 0 -- used for db-only */
	if(presentity->event->req_auth) {
		/* get rules_document */
		if(presentity->event->get_rules_doc(
				   &presentity->user, &presentity->domain, &rules_doc)
				< 0) {
			LM_ERR("getting rules doc\n");
			goto error;
		}
	}

	if(uandd_to_uri(presentity->user, presentity->domain, &pres_uri) < 0) {
		LM_ERR("constructing uri from user and domain\n");
		goto error;
	}

	ptc.user = presentity->user;
	ptc.domain = presentity->domain;
	ptc.event = presentity->event->name;
	ptc.etag = presentity->etag;

	ptm.user = presentity->user;
	ptm.domain = presentity->domain;
	ptm.event = presentity->event->name;
	ptm.etag = presentity->etag;

	if(new_t) {
		LM_DBG("new presentity with etag %.*s\n", presentity->etag.len,
				presentity->etag.s);

		if(ruid) {
			/* use the provided ruid */
			p_ruid = *ruid;
		} else {
			/* generate a new ruid */
			if(sruid_next(&pres_sruid) < 0)
				goto error;
			p_ruid = pres_sruid.uid;
		}
		if(presentity->sender) {
			ptc.sender = *presentity->sender;
		} else {
			ptc.sender.s = "";
			ptc.sender.len = 0;
		}
		ptc.body = *body;
		ptc.received_time = presentity->received_time;
		ptc.priority = presentity->priority;
		ptc.ruid = p_ruid;

		LM_DBG("adding new htable record\n");
		if(presentity->expires == -1) {
			replace = 1;
		}
		if(!replace) {
			/* A real PUBLISH */
			ptc.expires = presentity->expires + (int)time(NULL);
			if(presentity->event->evp->type == EVENT_DIALOG) {
				check_if_dialog(*body, &is_dialog, &dialog_id);
				if(dialog_id) {
					if(delete_presentity_if_dialog_id_exists(
							   presentity, dialog_id)
							< 0) {
						free(dialog_id);
						dialog_id = NULL;
						goto error;
					}

					free(dialog_id);
					dialog_id = NULL;
				}
			}
			LM_DBG("inserting presentity into hash table\n");
			if(ps_ptable_insert(&ptc) < 0) {
				LM_ERR("inserting new record in memory\n");
				goto error;
			}
		} else {
			/* A hard-state PUBLISH */
			ptc.expires = -1;
			if(presentity->expires != -1) {
				ptc.expires = presentity->expires + (int)time(NULL);
			}
			/* update/replace in memory */
			if(ps_ptable_replace(&ptm, &ptc) <0) {
				LM_ERR("replacing record in database\n");
				goto error;
			}
		}
		if(publ_send200ok(msg, presentity->expires, presentity->etag) < 0) {
			LM_ERR("sending 200OK\n");
			goto error;
		}
		if(sent_reply) {
			*sent_reply = 1;
		}
		goto send_notify;
	} else {
		LM_DBG("updating existing presentity with etag %.*s\n",
				presentity->etag.len, presentity->etag.s);

		if(ruid) {
			p_ruid = *ruid;
		}
		if(EVENT_DIALOG_SLA(presentity->event->evp)) {
			ptx = ps_ptable_get_item(&ptm.user, &ptm.domain, &ptm.event,
					&ptm.etag);
			if(ptx == NULL) {
				LM_DBG("presentity record not found\n");
				goto send_412;
			}
			cache_record_exists = 1;
			if(!p_ruid.s && ptx->ruid.s) {
				crt_ruid.len = ptx->ruid.len;
				crt_ruid.s = (char *)pkg_malloc(sizeof(char) * crt_ruid.len);
				if(!crt_ruid.s) {
					LM_ERR("no private memory\n");
					goto error;
				}
				memcpy(crt_ruid.s, ptx->ruid.s, crt_ruid.len);
				p_ruid = crt_ruid;
			}

			old_body = ptx->body;
			if(check_if_dialog(*body, &is_dialog, &dialog_id) < 0) {
				LM_ERR("failed to check if dialog stored\n");
				if(dialog_id) {
					free(dialog_id);
					dialog_id = NULL;
				}
				goto error;
			}

			free(dialog_id);

			if(is_dialog == 1) {
				/* if the new body has a dialog - overwrite */
				goto after_dialog_check;
			}
			if(check_if_dialog(old_body, &is_dialog, &old_dialog_id) < 0) {
				LM_ERR("failed to check if dialog stored\n");
				if(old_dialog_id) {
					free(old_dialog_id);
					old_dialog_id = NULL;
				}
				goto error;
			}

			/* if the old body has no dialog - overwrite */
			if(is_dialog == 0) {
				if(old_dialog_id) {
					free(old_dialog_id);
					old_dialog_id = NULL;
				}
				goto after_dialog_check;
			}

			if(old_dialog_id) {
				free(old_dialog_id);
				old_dialog_id = NULL;
			}

			sender = ptx->sender;

			LM_DBG("old_sender = %.*s\n", sender.len, sender.s);
			if(presentity->sender) {
				if(!(presentity->sender->len == sender.len
						   && presence_sip_uri_match(
									  presentity->sender, &sender)
									  == 0))
					bla_update_publish = 0;
			}
after_dialog_check:
			ps_presentity_free(ptx, 1);
			ptx = NULL;
		}

		if(presentity->expires <= 0) {

			if(!cache_record_exists) {
				ptx = ps_ptable_get_item(&ptm.user, &ptm.domain, &ptm.event,
						&ptm.etag);
				if(ptx == NULL) {
					LM_DBG("presentity record not found\n");
					goto send_412;
				}
				cache_record_exists = 1;
				if(!p_ruid.s && ptx->ruid.s) {
					crt_ruid.len = ptx->ruid.len;
					crt_ruid.s = (char *)pkg_malloc(sizeof(char) * crt_ruid.len);
					if(!crt_ruid.s) {
						LM_ERR("no private memory\n");
						goto error;
					}
					memcpy(crt_ruid.s, ptx->ruid.s, crt_ruid.len);
					p_ruid = crt_ruid;
				}
				ps_presentity_free(ptx, 1);
				ptx = NULL;
			}
			if(publ_send200ok(msg, presentity->expires, presentity->etag) < 0) {
				LM_ERR("sending 200OK reply\n");
				goto error;
			}
			if(sent_reply) {
				*sent_reply = 1;
			}

			if(publ_notify(presentity, pres_uri, body, &presentity->etag,
						   rules_doc)
						< 0) {
				LM_ERR("while sending notify\n");
				goto error;
			}
			if(num_watchers == 0) {
				if(delete_presentity(presentity, &p_ruid) < 0) {
					LM_ERR("Deleting presentity\n");
					goto error;
				}

				LM_DBG("deleted from db %.*s\n", presentity->user.len,
						presentity->user.s);
			}
			goto done;
		}

		/* if event dialog and is_dialog -> if sender not the same as
		 * old sender do not overwrite */
		if(EVENT_DIALOG_SLA(presentity->event->evp)
				&& bla_update_publish == 0) {
			LM_DBG("drop Publish for BLA from a different sender that"
				   " wants to overwrite an existing dialog\n");
			LM_DBG("sender = %.*s\n", presentity->sender->len,
					presentity->sender->s);
			if(publ_send200ok(msg, presentity->expires, presentity->etag) < 0) {
				LM_ERR("sending 200OK reply\n");
				goto error;
			}
			if(sent_reply)
				*sent_reply = 1;
			goto done;
		}

		if(presentity->event->etag_not_new == 0 || etag_override) {
			unsigned int publ_nr;
			str str_publ_nr = {0, 0};

			if(etag_override) {
				/* use the supplied etag */
				LM_DBG("updating with supplied etag %.*s\n", etag_override->len,
						etag_override->s);
				crt_etag = *etag_override;
				goto after_etag_generation;
			}

			/* generate another etag */
			LM_DBG("generating a new etag (%d)\n", presentity->event->etag_not_new);

			dot = presentity->etag.s + presentity->etag.len;
			while(*dot != '.' && str_publ_nr.len < presentity->etag.len) {
				str_publ_nr.len++;
				dot--;
			}
			if(str_publ_nr.len == presentity->etag.len) {
				LM_ERR("wrong etag\n");
				goto error;
			}
			str_publ_nr.s = dot + 1;
			str_publ_nr.len--;

			if(str2int(&str_publ_nr, &publ_nr) < 0) {
				LM_ERR("converting string to int\n");
				goto error;
			}
			etag.s = generate_ETag(publ_nr + 1);
			if(etag.s == NULL) {
				LM_ERR("while generating etag\n");
				goto error;
			}
			etag.len = (strlen(etag.s));

			crt_etag = etag;

after_etag_generation:
			ptc.etag = crt_etag;
		} else {
			crt_etag = presentity->etag;
		}

		if(presentity->event->evp->type == EVENT_DIALOG) {
			if(ps_match_dialog_state(presentity, "terminated") == 1) {
				LM_WARN("Trying to update an already terminated state."
						" Skipping update.\n");

				/* send 200OK */
				if(publ_send200ok(msg, presentity->expires, crt_etag) < 0) {
					LM_ERR("sending 200OK reply\n");
					goto error;
				}
				if(sent_reply) {
					*sent_reply = 1;
				}
				goto done;
			}
		}

		ptc.expires = presentity->expires + (int)time(NULL);
		ptc.received_time = presentity->received_time;
		ptc.priority = presentity->priority;
		if(body && body->s) {
			ptc.body = *body;
		}
		if(presentity->sender) {
			ptc.sender = *presentity->sender;
		}

		/* if there is no support for affected_rows and no previous query has been done,
		 * or dmq replication is enabled and we don't already know the ruid, do query */
		if((!cache_record_exists)
				|| (pres_enable_dmq > 0 && !p_ruid.s)) {
			ptx = ps_ptable_get_item(&ptm.user, &ptm.domain, &ptm.event,
					&ptm.etag);
			if(ptx == NULL) {
				LM_DBG("presentity record not found\n");
				goto send_412;
			}
			cache_record_exists = 1;
			if(!p_ruid.s && ptx->ruid.s) {
				crt_ruid.len = ptx->ruid.len;
				crt_ruid.s = (char *)pkg_malloc(sizeof(char) * crt_ruid.len);
				if(!crt_ruid.s) {
					LM_ERR("no private memory\n");
					goto error;
				}
				memcpy(crt_ruid.s, ptx->ruid.s, crt_ruid.len);
				p_ruid = crt_ruid;
			}
			LM_DBG("existing ruid %.*s\n", p_ruid.len, p_ruid.s);
			ps_presentity_free(ptx, 1);
			ptx = NULL;
		}
		affected_rows = ps_ptable_update(&ptm, &ptc);
		if(affected_rows < 0) {
			LM_ERR("updating published info in database\n");
			goto error;
		}
		/* if either affected_rows (if exists) or select query show that there is no line in database*/
		if(affected_rows==0) {
			LM_DBG("no presentity record found to be updated\n");
			goto send_412;
		}

		/* send 200OK */
		if(publ_send200ok(msg, presentity->expires, crt_etag) < 0) {
			LM_ERR("sending 200OK reply\n");
			goto error;
		}
		if(sent_reply) {
			*sent_reply = 1;
		}

		if(!body) {
			goto done;
		}
	}

send_notify:

	/* send notify with presence information */
	if(publ_notify(presentity, pres_uri, body, NULL, rules_doc) < 0) {
		LM_ERR("while sending notify\n");
		goto error;
	}

done:

	if(pres_enable_dmq > 0 && p_ruid.s != NULL) {
		pres_dmq_replicate_presentity(
				presentity, body, new_t, &crt_etag, sphere, &p_ruid, NULL);
	}

	if(etag.s) {
		pkg_free(etag.s);
	}
	etag.s = NULL;

	if(crt_ruid.s) {
		pkg_free(crt_ruid.s);
	}
	crt_ruid.s = NULL;

	if(rules_doc) {
		if(rules_doc->s) {
			pkg_free(rules_doc->s);
		}
		pkg_free(rules_doc);
		rules_doc = NULL;
	}
	if(pres_uri.s) {
		pkg_free(pres_uri.s);
		pres_uri.s = NULL;
	}

	return 0;

send_412:

	if(!ruid) {
		LM_ERR("No E_Tag match %*s\n", presentity->etag.len,
				presentity->etag.s);
	} else {
		LM_ERR("No ruid match %*s\n", ruid->len, ruid->s);
	}

	if(msg != NULL) {
		if(slb.freply(msg, 412, &pu_412_rpl) < 0) {
			LM_ERR("sending '412 Conditional request failed' reply\n");
			goto error;
		}
	}
	if(sent_reply) {
		*sent_reply = 1;
	}
	ret = 0;

error:
	if(ptx) {
		ps_presentity_free(ptx, 1);
	}
	if(etag.s) {
		pkg_free(etag.s);
	}
	if(rules_doc) {
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}
	if(pres_uri.s) {
		pkg_free(pres_uri.s);
	}
	if(crt_ruid.s) {
		pkg_free(crt_ruid.s);
	}

	return ret;
}

/**
 *
 */
int update_presentity(sip_msg_t *msg, presentity_t *presentity, str *body,
		int new_t, int *sent_reply, char *sphere, str *etag_override, str *ruid,
		int replace)
{
	if(publ_cache_mode == PS_PCACHE_RECORD) {
		return ps_cache_update_presentity(msg, presentity, body, new_t,
				sent_reply, sphere, etag_override, ruid, replace);
	} else {
		return ps_db_update_presentity(msg, presentity, body, new_t,
				sent_reply, sphere, etag_override, ruid, replace);
	}
}

/**
 *
 */
int pres_htable_db_restore(void)
{
	/* query all records from presentity table and insert records
	 * in presentity table */
	db_key_t result_cols[6];
	db1_res_t *result = NULL;
	db_row_t *row = NULL;
	db_val_t *row_vals;
	int i;
	str user, domain, ev_str, uri, body;
	int n_result_cols = 0;
	int user_col, domain_col, event_col, expires_col, body_col = 0;
	int event;
	event_t ev;
	char *sphere = NULL;
	static str query_str;

	result_cols[user_col = n_result_cols++] = &str_username_col;
	result_cols[domain_col = n_result_cols++] = &str_domain_col;
	result_cols[event_col = n_result_cols++] = &str_event_col;
	result_cols[expires_col = n_result_cols++] = &str_expires_col;
	if(pres_sphere_enable) {
		result_cols[body_col = n_result_cols++] = &str_body_col;
	}

	if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
		LM_ERR("unsuccessful use table sql operation\n");
		goto error;
	}

	query_str = str_username_col;
	if(db_fetch_query(&pa_dbf, pres_fetch_rows, pa_db, 0, 0, 0, result_cols, 0,
			   n_result_cols, &query_str, &result)
			< 0) {
		LM_ERR("querying presentity\n");
		goto error;
	}
	if(result == NULL)
		goto error;

	if(result->n <= 0) {
		pa_dbf.free_result(pa_db, result);
		return 0;
	}

	do {
		for(i = 0; i < result->n; i++) {
			row = &result->rows[i];
			row_vals = ROW_VALUES(row);

			if(pres_startup_mode != 0
					&& (row_vals[expires_col].val.int_val < (int)time(NULL)))
				continue;

			sphere = NULL;
			user.s = (char *)row_vals[user_col].val.string_val;
			user.len = strlen(user.s);
			domain.s = (char *)row_vals[domain_col].val.string_val;
			domain.len = strlen(domain.s);
			ev_str.s = (char *)row_vals[event_col].val.string_val;
			ev_str.len = strlen(ev_str.s);

			memset(&ev, 0, sizeof(event_t));
			if(event_parser(ev_str.s, ev_str.len, &ev) < 0) {
				LM_ERR("parsing event\n");
				free_event_params(ev.params.list, PKG_MEM_TYPE);
				goto error;
			}
			event = ev.type;
			free_event_params(ev.params.list, PKG_MEM_TYPE);

			if(uandd_to_uri(user, domain, &uri) < 0) {
				LM_ERR("constructing uri\n");
				goto error;
			}
			/* insert in hash_table*/

			if(pres_sphere_enable && event == EVENT_PRESENCE) {
				body.s = (char *)row_vals[body_col].val.string_val;
				body.len = strlen(body.s);
				sphere = extract_sphere(&body);
			}

			if(insert_phtable(&uri, event, sphere) < 0) {
				LM_ERR("inserting record in presentity hash table");
				pkg_free(uri.s);
				if(sphere)
					pkg_free(sphere);
				goto error;
			}
			if(sphere)
				pkg_free(sphere);
			pkg_free(uri.s);
		}
	} while((db_fetch_next(&pa_dbf, pres_fetch_rows, pa_db, &result) == 1)
			&& (RES_ROW_N(result) > 0));

	pa_dbf.free_result(pa_db, result);

	return 0;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	return -1;
}

char *extract_sphere(str *body)
{
	/* check for a rpid sphere element */
	xmlDocPtr doc = NULL;
	xmlNodePtr node;
	char *cont, *sphere = NULL;


	doc = xmlParseMemory(body->s, body->len);
	if(doc == NULL) {
		LM_ERR("failed to parse xml body\n");
		return NULL;
	}

	node = xmlNodeGetNodeByName(doc->children, "sphere", "rpid");

	if(node == NULL)
		node = xmlNodeGetNodeByName(doc->children, "sphere", "r");

	if(node) {
		LM_DBG("found sphere definition\n");
		cont = (char *)xmlNodeGetContent(node);
		if(cont == NULL) {
			LM_ERR("failed to extract sphere node content\n");
			goto error;
		}
		sphere = (char *)pkg_malloc((strlen(cont) + 1) * sizeof(char));
		if(sphere == NULL) {
			xmlFree(cont);
			ERR_MEM(PKG_MEM_STR);
		}
		strcpy(sphere, cont);
		xmlFree(cont);
	} else
		LM_DBG("didn't find sphere definition\n");

error:
	xmlFreeDoc(doc);

	return sphere;
}

xmlNodePtr xmlNodeGetNodeByName(
		xmlNodePtr node, const char *name, const char *ns)
{
	xmlNodePtr cur = node;
	while(cur) {
		xmlNodePtr match = NULL;
		if(xmlStrcasecmp(cur->name, (unsigned char *)name) == 0) {
			if(!ns
					|| (cur->ns
							   && xmlStrcasecmp(
										  cur->ns->prefix, (unsigned char *)ns)
										  == 0))
				return cur;
		}
		match = xmlNodeGetNodeByName(cur->children, name, ns);
		if(match)
			return match;
		cur = cur->next;
	}
	return NULL;
}

char *ps_db_get_sphere(str *pres_uri)
{
	unsigned int hash_code;
	char *sphere = NULL;
	pres_entry_t *p;
	db_key_t query_cols[6];
	db_val_t query_vals[6];
	db_key_t result_cols[6];
	db1_res_t *result = NULL;
	db_row_t *row = NULL;
	db_val_t *row_vals;
	int n_result_cols = 0;
	int n_query_cols = 0;
	struct sip_uri uri;
	str body;
	static str query_str;


	if(!pres_sphere_enable) {
		return NULL;
	}

	if(publ_cache_mode == PS_PCACHE_HYBRID) {
		/* search in hash table*/
		hash_code = core_case_hash(pres_uri, NULL, phtable_size);

		lock_get(&pres_htable[hash_code].lock);

		p = search_phtable(pres_uri, EVENT_PRESENCE, hash_code);

		if(p) {
			if(p->sphere) {
				sphere = (char *)pkg_malloc(strlen(p->sphere) * sizeof(char));
				if(sphere == NULL) {
					lock_release(&pres_htable[hash_code].lock);
					ERR_MEM(PKG_MEM_STR);
				}
				strcpy(sphere, p->sphere);
			}
			lock_release(&pres_htable[hash_code].lock);
			return sphere;
		}
		lock_release(&pres_htable[hash_code].lock);
	}

	if(parse_uri(pres_uri->s, pres_uri->len, &uri) < 0) {
		LM_ERR("failed to parse presentity uri\n");
		goto error;
	}

	query_cols[n_query_cols] = &str_domain_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = uri.host;
	n_query_cols++;

	query_cols[n_query_cols] = &str_username_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = uri.user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_event_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val.s = "presence";
	query_vals[n_query_cols].val.str_val.len = 8;
	n_query_cols++;

	result_cols[n_result_cols++] = &str_body_col;

	if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
		LM_ERR("in use_table\n");
		return NULL;
	}

	if(pres_retrieve_order == 1) {
		query_str = pres_retrieve_order_by;
	} else {
		query_str = str_received_time_col;
	}
	if(pa_dbf.query(pa_db, query_cols, 0, query_vals, result_cols, n_query_cols,
			   n_result_cols, &query_str, &result)
			< 0) {
		LM_ERR("failed to query %.*s table\n", presentity_table.len,
				presentity_table.s);
		if(result)
			pa_dbf.free_result(pa_db, result);
		return NULL;
	}

	if(result == NULL)
		return NULL;

	if(result->n <= 0) {
		LM_DBG("no published record found in database\n");
		pa_dbf.free_result(pa_db, result);
		return NULL;
	}

	row = &result->rows[result->n - 1];
	row_vals = ROW_VALUES(row);
	if(row_vals[0].val.string_val == NULL) {
		LM_ERR("NULL notify body record\n");
		goto error;
	}

	body.s = (char *)row_vals[0].val.string_val;
	body.len = strlen(body.s);
	if(body.len == 0) {
		LM_ERR("Empty notify body record\n");
		goto error;
	}

	sphere = extract_sphere(&body);

	pa_dbf.free_result(pa_db, result);

	return sphere;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	return NULL;
}

char *ps_cache_get_sphere(str *pres_uri)
{
	char *sphere = NULL;
	sip_uri_t uri;
	ps_presentity_t ptm;
	ps_presentity_t *ptx = NULL;
	ps_presentity_t *ptlist = NULL;

	if(!pres_sphere_enable) {
		return NULL;
	}

	if(parse_uri(pres_uri->s, pres_uri->len, &uri) < 0) {
		LM_ERR("failed to parse presentity uri\n");
		return NULL;
	}

	memset(&ptm, 0, sizeof(ps_presentity_t));

	ptm.user = uri.user;
	ptm.domain = uri.host;
	ptm.event.s = "presence";
	ptm.event.len = 8;

	ptlist = ps_ptable_search(&ptm, 1, pres_retrieve_order);

	if(ptlist == NULL) {
		return NULL;
	}
	ptx = ptlist;
	while(ptx->next) {
		ptx = ptx->next;
	}

	if(ptx->body.s==NULL || ptx->body.len<=0) {
		ps_presentity_list_free(ptlist, 1);
		return NULL;
	}

	sphere = extract_sphere(&ptx->body);
	ps_presentity_list_free(ptlist, 1);

	return sphere;
}

char *get_sphere(str *pres_uri)
{
	if(publ_cache_mode == PS_PCACHE_RECORD) {
		return ps_cache_get_sphere(pres_uri);
	} else {
		return ps_db_get_sphere(pres_uri);
	}
}

int mark_presentity_for_delete(presentity_t *pres, str *ruid)
{
	db_key_t query_cols[4], result_cols[1], update_cols[3];
	db_val_t query_vals[4], update_vals[3], *value;
	db_row_t *row;
	db1_res_t *result = NULL;
	int n_query_cols = 0, n_update_cols = 0;
	int ret = -1;
	str *cur_body = NULL, *new_body = NULL;
	db_query_f query_fn = pa_dbf.query_lock ? pa_dbf.query_lock : pa_dbf.query;

	if(pres->event->agg_nbody == NULL) {
		/* Nothing clever to do here... just delete */
		if(delete_presentity(pres, NULL) < 0) {
			LM_ERR("deleting presentity\n");
			goto error;
		}
		goto done;
	}

	if(pa_db==NULL) {
		LM_ERR("no database connection setup\n");
		goto error;
	}

	if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
		LM_ERR("unsuccessful use table sql operation\n");
		goto error;
	}

	if(!ruid) {
		query_cols[n_query_cols] = &str_username_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = pres->user;
		n_query_cols++;

		query_cols[n_query_cols] = &str_domain_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = pres->domain;
		n_query_cols++;

		query_cols[n_query_cols] = &str_event_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = pres->event->name;
		n_query_cols++;

		query_cols[n_query_cols] = &str_etag_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = pres->etag;
		n_query_cols++;
	} else {
		query_cols[n_query_cols] = &str_ruid_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = *ruid;
		n_query_cols++;
	}

	result_cols[0] = &str_body_col;

	if(query_fn(pa_db, query_cols, 0, query_vals, result_cols, n_query_cols, 1,
			   0, &result)
			< 0) {
		LM_ERR("query failed\n");
		goto error;
	}

	if(result == NULL) {
		LM_ERR("bad result\n");
		goto error;
	}

	if(RES_ROW_N(result) <= 0) {
		/* Can happen when the timer and notifier processes clash */
		LM_INFO("Found 0 presentities - expected 1\n");
		goto done;
	}

	if(RES_ROW_N(result) > 1) {
		/* More that one is prevented by DB constraint  - but handle
		 * it anyway */
		LM_ERR("Found %d presentities - expected 1\n", RES_ROW_N(result));

		if(delete_presentity(pres, ruid) < 0) {
			LM_ERR("deleting presentity\n");
			goto error;
		}

		/* Want the calling function to continue properly so do not
		 * return an error */
		goto done;
	}

	row = RES_ROWS(result);
	value = ROW_VALUES(row);

	if((cur_body = (str *)pkg_malloc(sizeof(str))) == NULL) {
		LM_ERR("allocating pkg memory\n");
		goto error;
	}
	cur_body->s = (char *)value->val.string_val;
	cur_body->len = strlen(cur_body->s);
	if((new_body = pres->event->agg_nbody(
				&pres->user, &pres->domain, &cur_body, 1, 0))
			== NULL) {
		LM_ERR("preparing body\n");
		goto error;
	}

	update_cols[n_update_cols] = &str_etag_col;
	update_vals[n_update_cols].type = DB1_STR;
	update_vals[n_update_cols].nul = 0;
	update_vals[n_update_cols].val.str_val = str_offline_etag_val;
	n_update_cols++;

	update_cols[n_update_cols] = &str_expires_col;
	update_vals[n_update_cols].type = DB1_INT;
	update_vals[n_update_cols].nul = 0;
	update_vals[n_update_cols].val.int_val = (int)time(NULL);
	n_update_cols++;

	update_cols[n_update_cols] = &str_body_col;
	update_vals[n_update_cols].type = DB1_STR;
	update_vals[n_update_cols].nul = 0;
	update_vals[n_update_cols].val.str_val.s = new_body->s;
	update_vals[n_update_cols].val.str_val.len = new_body->len;
	n_update_cols++;

	if(pa_dbf.update(pa_db, query_cols, 0, query_vals, update_cols, update_vals,
			   n_query_cols, n_update_cols)
			< 0) {
		LM_ERR("unsuccessful sql update operation");
		goto error;
	}

	if(pa_dbf.affected_rows)
		ret = pa_dbf.affected_rows(pa_db);
	else
	done:
		ret = 0;

error:
	free_notify_body(new_body, pres->event);
	if(cur_body)
		pkg_free(cur_body);
	if(result)
		pa_dbf.free_result(pa_db, result);

	return ret;
}

/**
 *
 */
int ps_db_delete_presentity(presentity_t *pres, str *ruid)
{
	db_key_t query_cols[4];
	db_val_t query_vals[4];
	int n_query_cols = 0;

	if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
		LM_ERR("unsuccessful use table sql operation\n");
		goto error;
	}

	if(!ruid) {
		query_cols[n_query_cols] = &str_username_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = pres->user;
		n_query_cols++;

		query_cols[n_query_cols] = &str_domain_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = pres->domain;
		n_query_cols++;

		query_cols[n_query_cols] = &str_event_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = pres->event->name;
		n_query_cols++;

		query_cols[n_query_cols] = &str_etag_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = pres->etag;
		n_query_cols++;
	} else {
		query_cols[n_query_cols] = &str_ruid_col;
		query_vals[n_query_cols].type = DB1_STR;
		query_vals[n_query_cols].nul = 0;
		query_vals[n_query_cols].val.str_val = *ruid;
		n_query_cols++;
	}

	if(pa_dbf.delete(pa_db, query_cols, 0, query_vals, n_query_cols) < 0) {
		LM_ERR("unsuccessful sql delete operation");
		goto error;
	}

	if(pa_dbf.affected_rows)
		return pa_dbf.affected_rows(pa_db);
	else
		return 0;

error:
	return -1;
}

/**
 *
 */
int ps_cache_delete_presentity(presentity_t *pres, str *ruid)
{
	ps_presentity_t ptc;

	memset(&ptc, 0, sizeof(ps_presentity_t));

	ptc.user = pres->user;
	ptc.domain = pres->domain;
	ptc.event = pres->event->name;
	ptc.etag = pres->etag;

	if(ps_ptable_remove(&ptc) < 0) {
		return -1;
	}

	return 0;
}

/**
 *
 */
int delete_presentity(presentity_t *pres, str *ruid)
{
	if(publ_cache_mode == PS_PCACHE_RECORD) {
		return ps_cache_delete_presentity(pres, ruid);
	} else {
		return ps_db_delete_presentity(pres, ruid);
	}
}

int delete_offline_presentities(str *pres_uri, pres_ev_t *event)
{
	db_key_t query_cols[4];
	db_val_t query_vals[4];
	int n_query_cols = 0;
	struct sip_uri uri;

	if(pa_db==NULL) {
		LM_ERR("no database connection setup\n");
		goto error;
	}

	if(parse_uri(pres_uri->s, pres_uri->len, &uri) < 0) {
		LM_ERR("failed to parse presentity uri\n");
		goto error;
	}

	query_cols[n_query_cols] = &str_username_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = uri.user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_domain_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = uri.host;
	n_query_cols++;

	query_cols[n_query_cols] = &str_event_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = event->name;
	n_query_cols++;

	query_cols[n_query_cols] = &str_etag_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = str_offline_etag_val;
	n_query_cols++;

	if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
		LM_ERR("unsuccessful use table sql operation\n");
		goto error;
	}

	if(pa_dbf.delete(pa_db, query_cols, 0, query_vals, n_query_cols) < 0) {
		LM_ERR("unsuccessful sql delete operation");
		goto error;
	}

	if(pa_dbf.affected_rows)
		return pa_dbf.affected_rows(pa_db);
	else
		return 0;

error:
	return -1;
}

// used for API updates to the presentity table
int _api_update_presentity(str *event, str *realm, str *user, str *etag,
		str *sender, str *body, int expires, int new_t, int replace)
{
	int ret = -1;
	presentity_t *pres = NULL;
	pres_ev_t *ev = NULL;
	char *sphere = NULL;

	ev = contains_event(event, NULL);
	if(ev == NULL) {
		LM_ERR("wrong event parameter\n");
		return -1;
	}

	pres = new_presentity(realm, user, expires, ev, etag, sender);

	if(pres_sphere_enable) {
		sphere = extract_sphere(body);
	}
	if(pres) {
		ret = update_presentity(
				NULL, pres, body, new_t, NULL, sphere, NULL, NULL, replace);
		pkg_free(pres);
	}

	if(sphere)
		pkg_free(sphere);

	return ret;
}
