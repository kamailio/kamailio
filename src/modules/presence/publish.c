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
 * \brief Kamailio presence module :: Support for PUBLISH handling
 * \ingroup presence
 */


#include <time.h>

#include "../../core/ut.h"
#include "../../core/str.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_expires.h"
#include "../../core/parser/parse_event.h"
#include "../../core/parser/parse_content.h"
#include "../../core/lock_ops.h"
#include "../../core/hashes.h"
#include "../../core/strutils.h"
#include "../../core/mod_fix.h"
#include "../../lib/srdb1/db.h"
#include "presence.h"
#include "notify.h"
#include "utils_func.h"
#include "publish.h"
#include "presentity.h"

extern gen_lock_set_t *set;

static str pu_400a_rpl = str_init("Bad request");
static str pu_400b_rpl = str_init("Invalid request");
static str pu_500_rpl = str_init("Server Internal Error");
static str pu_489_rpl = str_init("Bad Event");

struct p_modif
{
	presentity_t *p;
	str uri;
};

void ps_presentity_db_timer_clean(unsigned int ticks, void *param)
{
	db_key_t db_keys[2], result_cols[4];
	db_val_t db_vals[2], *values;
	db_op_t db_ops[2];
	db1_res_t *result = NULL;
	db_row_t *rows;
	int n_db_cols = 0, n_result_cols = 0;
	int event_col, etag_col, user_col, domain_col;
	int i = 0, num_watchers = 0;
	pres_ev_t fake;
	presentity_t pres;
	str uri = {0, 0}, event, *rules_doc = NULL;
	static str query_str;

	if(pa_db == NULL) {
		return;
	}

	LM_DBG("cleaning expired presentity information\n");
	if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
		LM_ERR("in use_table\n");
		return;
	}

	db_keys[n_db_cols] = &str_expires_col;
	db_ops[n_db_cols] = OP_LT;
	db_vals[n_db_cols].type = DB1_INT;
	db_vals[n_db_cols].nul = 0;
	db_vals[n_db_cols].val.int_val = (int)time(NULL);
	n_db_cols++;

	db_keys[n_db_cols] = &str_expires_col;
	db_ops[n_db_cols] = OP_GT;
	db_vals[n_db_cols].type = DB1_INT;
	db_vals[n_db_cols].nul = 0;
	db_vals[n_db_cols].val.int_val = 0;
	n_db_cols++;

	result_cols[user_col = n_result_cols++] = &str_username_col;
	result_cols[domain_col = n_result_cols++] = &str_domain_col;
	result_cols[etag_col = n_result_cols++] = &str_etag_col;
	result_cols[event_col = n_result_cols++] = &str_event_col;

	query_str = str_username_col;
	if(db_fetch_query(&pa_dbf, pres_fetch_rows, pa_db, db_keys, db_ops, db_vals,
			   result_cols, n_db_cols, n_result_cols, &query_str, &result)
			< 0) {
		LM_ERR("failed to query database for expired messages\n");
		goto delete_pres;
	}

	if(result == NULL) {
		LM_ERR("bad result\n");
		return;
	}

	LM_DBG("found n= %d expires messages\n ", result->n);

	do {
		rows = RES_ROWS(result);

		for(i = 0; i < RES_ROW_N(result); i++) {
			num_watchers = 0;
			values = ROW_VALUES(&rows[i]);
			memset(&pres, 0, sizeof(presentity_t));

			pres.user.s = (char *)VAL_STRING(&values[user_col]);
			pres.user.len = strlen(pres.user.s);
			pres.domain.s = (char *)VAL_STRING(&values[domain_col]);
			pres.domain.len = strlen(pres.domain.s);
			pres.etag.s = (char *)VAL_STRING(&values[etag_col]);
			pres.etag.len = strlen(pres.etag.s);
			event.s = (char *)VAL_STRING(&values[event_col]);
			event.len = strlen(event.s);
			pres.event = contains_event(&event, NULL);
			if(pres.event == NULL || pres.event->evp == NULL) {
				LM_ERR("event[%.*s] not found\n", STR_FMT(&event));
				memset(&fake, 0, sizeof(pres_ev_t));
				fake.name = event;
				pres.event = &fake;
				goto simple_error;
			}

			if(uandd_to_uri(pres.user, pres.domain, &uri) < 0) {
				LM_ERR("constructing uri from [user]=%.*s  [domain]=%.*s\n",
					STR_FMT(&pres.user), STR_FMT(&pres.domain));
				goto simple_error;
			}

			/* delete from hash table */
			if(publ_cache_mode==PS_PCACHE_HYBRID
					&& delete_phtable(&uri, pres.event->evp->type) < 0) {
				LM_ERR("deleting uri[%.*s] event[%.*s] from presentity hash table\n",
					STR_FMT(&uri), STR_FMT(&event));
				goto simple_error;
			}

			LM_DBG("found expired publish for [user]=%.*s  [domain]=%.*s\n",
					pres.user.len, pres.user.s, pres.domain.len, pres.domain.s);

			if(pres_force_delete == 1) {
				if(delete_presentity(&pres, NULL) < 0) {
					LM_ERR("Deleting presentity uri[%.*s]\n", STR_FMT(&uri));
					goto error;
				}
			} else if(pres_notifier_processes > 0) {
				if(pa_dbf.start_transaction) {
					if(pa_dbf.start_transaction(pa_db, pres_db_table_lock) < 0) {
						LM_ERR("in start_transaction\n");
						goto error;
					}
				}
				if((num_watchers = publ_notify_notifier(uri, pres.event)) < 0) {
					LM_ERR("Updating watcher records\n");
					if(pa_dbf.abort_transaction) {
						if(pa_dbf.abort_transaction(pa_db) < 0)
							LM_ERR("in abort_transaction\n");
					}
					goto next;
				}

				if(num_watchers > 0) {
					if(mark_presentity_for_delete(&pres, NULL) < 0) {
						LM_ERR("Marking presentity\n");
						if(pa_dbf.abort_transaction) {
							if(pa_dbf.abort_transaction(pa_db) < 0)
								LM_ERR("in abort_transaction\n");
						}
						goto next;
					}
				} else {
					if(delete_presentity(&pres, NULL) < 0) {
						LM_ERR("Deleting presentity uri[%.*s]\n", STR_FMT(&uri));
						goto next;
					}
				}
				if(pa_dbf.end_transaction) {
					if(pa_dbf.end_transaction(pa_db) < 0) {
						LM_ERR("in end_transaction\n");
						goto error;
					}
				}
			} else {
				if(pres.event->get_rules_doc
						&& pres.event->get_rules_doc(
								   &pres.user, &pres.domain, &rules_doc)
								   < 0) {
					LM_ERR("getting rules doc\n");
					goto simple_error;
				}
				if(publ_notify(&pres, uri, NULL, &pres.etag, rules_doc) < 0) {
					LM_ERR("sending Notify request\n");
					goto simple_error;
				}
			}

simple_error:
			if(num_watchers == 0 && delete_presentity(&pres, NULL) < 0) {
				LM_ERR("Deleting presentity\n");
			}
next:
			if(uri.s) {
				pkg_free(uri.s);
				uri.s = NULL;
			}
			if(rules_doc) {
				if(rules_doc->s) {
					pkg_free(rules_doc->s);
				}
				pkg_free(rules_doc);
				rules_doc = NULL;
			}
		}
	} while(db_fetch_next(&pa_dbf, pres_fetch_rows, pa_db, &result) == 1
			&& RES_ROW_N(result) > 0);

	pa_dbf.free_result(pa_db, result);
	result = NULL;

	if(pa_dbf.use_table(pa_db, &presentity_table) < 0) {
		LM_ERR("in use_table\n");
		goto error;
	}

	if(pres_notifier_processes == 0) {
	delete_pres:
		if(pa_dbf.delete(pa_db, db_keys, db_ops, db_vals, n_db_cols) < 0)
			LM_ERR("failed to delete expired records from DB\n");
	}

	return;

error:
	if(result)
		pa_dbf.free_result(pa_db, result);
	if(uri.s)
		pkg_free(uri.s);
	if(rules_doc) {
		if(rules_doc->s)
			pkg_free(rules_doc->s);
		pkg_free(rules_doc);
	}

	return;
}

/**
 *
 */
void ps_ptable_timer_clean(unsigned int ticks, void *param)
{
	presentity_t pres;
	ps_presentity_t *ptlist = NULL;
	ps_presentity_t *ptn = NULL;
	int eval = 0;
	str uri = STR_NULL;
	str *rules_doc = NULL;

	eval = (int)time(NULL);
	ptlist = ps_ptable_get_expired(eval);

	if(ptlist==NULL) {
		return;
	}
	for(ptn = ptlist; ptn != NULL; ptn = ptn->next) {
		memset(&pres, 0, sizeof(presentity_t));

		pres.user = ptn->user;
		pres.domain = ptn->domain;
		pres.etag = ptn->etag;
		pres.event = contains_event(&ptn->event, NULL);
		if(pres.event == NULL || pres.event->evp == NULL) {
			LM_ERR("event[%.*s] not found\n", STR_FMT(&ptn->event));
			goto next;
		}

		if(uandd_to_uri(pres.user, pres.domain, &uri) < 0) {
			LM_ERR("constructing uri from [user]=%.*s  [domain]=%.*s\n",
				STR_FMT(&pres.user), STR_FMT(&pres.domain));
			goto next;
		}

		LM_DBG("found expired publish for [user]=%.*s  [domain]=%.*s\n",
				pres.user.len, pres.user.s, pres.domain.len, pres.domain.s);

		if(pres_force_delete == 1) {
			if(ps_ptable_remove(ptn) <0) {
				LM_ERR("Deleting presentity\n");
				goto next;
			}
		} else {
			if(pres.event->get_rules_doc
					&& pres.event->get_rules_doc(
								&pres.user, &pres.domain, &rules_doc)
								< 0) {
				LM_ERR("getting rules doc\n");
				goto next;
			}
			if(publ_notify(&pres, uri, NULL, &pres.etag, rules_doc) < 0) {
				LM_ERR("sending Notify request\n");
				goto next;
			}
		}

next:
		if(uri.s) {
			pkg_free(uri.s);
			uri.s = NULL;
		}
		if(rules_doc) {
			if(rules_doc->s) {
				pkg_free(rules_doc->s);
			}
			pkg_free(rules_doc);
			rules_doc = NULL;
		}
	}

	for(ptn = ptlist; ptn != NULL; ptn = ptn->next) {
		if(ps_ptable_remove(ptn) <0) {
			LM_ERR("failed deleting presentity item\n");
		}
	}

	if(ptlist != NULL) {
		ps_presentity_list_free(ptlist, 1);
	}
	if(uri.s) {
		pkg_free(uri.s);
	}
	if(rules_doc) {
		if(rules_doc->s) {
			pkg_free(rules_doc->s);
		}
		pkg_free(rules_doc);
	}
	return;
}

/**
 * PUBLISH request handling
 *
 */
int ki_handle_publish_uri(struct sip_msg *msg, str *sender_uri)
{
	struct sip_uri puri;
	str body;
	int lexpire;
	presentity_t *presentity = 0;
	struct hdr_field *hdr;
	int found = 0, etag_gen = 0;
	str etag = {0, 0};
	str *sender = NULL;
	static char buf[256];
	int buf_len = 255;
	pres_ev_t *event = NULL;
	str pres_user;
	str pres_domain;
	int reply_code;
	str reply_str;
	int sent_reply = 0;
	char *sphere = NULL;

	reply_code = 500;
	reply_str = pu_500_rpl;

	pres_counter++;
	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("parsing headers\n");
		reply_code = 400;
		reply_str = pu_400a_rpl;
		goto error;
	}
	memset(&body, 0, sizeof(str));

	/* inspecting the Event header field */

	if(msg->event && msg->event->body.len > 0) {
		if(!msg->event->parsed && (parse_event(msg->event) < 0)) {
			LM_ERR("cannot parse Event header\n");
			reply_code = 400;
			reply_str = pu_400a_rpl;
			goto error;
		}
	} else {
		goto unsupported_event;
	}

	/* search event in the list */
	event = search_event((event_t *)msg->event->parsed);
	if(event == NULL) {
		goto unsupported_event;
	}

	/* examine the SIP-If-Match header field */
	hdr = msg->headers;
	while(hdr != NULL) {
		if(cmp_hdrname_strzn(&hdr->name, "SIP-If-Match", 12) == 0) {
			found = 1;
			break;
		}
		hdr = hdr->next;
	}
	if(found == 0) {
		LM_DBG("SIP-If-Match header not found\n");
		etag.s = generate_ETag(0);
		if(etag.s == NULL) {
			LM_ERR("when generating etag\n");
			goto error;
		}
		etag.len = (strlen(etag.s));
		etag_gen = 1;
		LM_DBG("new etag  = %.*s \n", etag.len, etag.s);
	} else {
		LM_DBG("SIP-If-Match header found\n");
		etag.s = (char *)pkg_malloc((hdr->body.len + 1) * sizeof(char));
		if(etag.s == NULL) {
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(etag.s, hdr->body.s, hdr->body.len);
		etag.len = hdr->body.len;
		etag.s[etag.len] = '\0';
		LM_DBG("existing etag  = %.*s \n", etag.len, etag.s);
	}

	/* examine the expire header field */
	if(msg->expires && msg->expires->body.len > 0) {
		if(!msg->expires->parsed && (parse_expires(msg->expires) < 0)) {
			LM_ERR("cannot parse Expires header\n");
			goto error;
		}
		lexpire = ((exp_body_t *)msg->expires->parsed)->val;
		LM_DBG("Expires header found, value= %d\n", lexpire);

	} else {
		LM_DBG("'expires' not found; default=%d\n", event->default_expires);
		lexpire = event->default_expires;
	}
	if(lexpire > pres_max_expires) {
		lexpire = pres_max_expires;
	}

	/* get pres_uri from Request-URI*/
	if(parse_sip_msg_uri(msg) < 0) {
		LM_ERR("parsing Request URI\n");
		reply_code = 400;
		reply_str = pu_400a_rpl;
		goto error;
	}
	pres_user = msg->parsed_uri.user;
	pres_domain = msg->parsed_uri.host;

	if(!msg->content_length) {
		LM_ERR("no Content-Length header found!\n");
		reply_code = 400;
		reply_str = pu_400a_rpl;
		goto error;
	}

	/* process the body */
	if(get_content_length(msg) == 0) {
		body.s = NULL;
		if(etag_gen) {
			LM_ERR("No E-Tag and no body found\n");
			reply_code = 400;
			reply_str = pu_400b_rpl;
			goto error;
		}
	} else {
		body.s = get_body(msg);
		if(body.s == NULL) {
			LM_ERR("cannot extract body\n");
			reply_code = 400;
			reply_str = pu_400a_rpl;
			goto error;
		}
		body.len = msg->buf + msg->len - body.s;

		if(pres_sphere_enable && event->evp->type == EVENT_PRESENCE
				&& get_content_type(msg) == SUBTYPE_PIDFXML) {
			sphere = extract_sphere(&body);
		}
	}
	memset(&puri, 0, sizeof(struct sip_uri));
	if(sender_uri) {
		sender = (str *)pkg_malloc(sizeof(str));
		if(sender == NULL) {
			ERR_MEM(PKG_MEM_STR);
		}
		if(sender_uri->len >= buf_len - 1) {
			LM_ERR("cannot use sender uri -- too long value\n");
			goto error;
		}
		strncpy(buf, sender_uri->s, sender_uri->len);
		buf_len = sender_uri->len;
		buf[buf_len] = '\0';
		if(parse_uri(buf, buf_len, &puri) != 0) {
			LM_ERR("bad sender SIP address!\n");
			reply_code = 400;
			reply_str = pu_400a_rpl;
			goto error;
		} else {
			LM_DBG("using user id [%.*s]\n", buf_len, buf);
		}
		sender->s = buf;
		sender->len = buf_len;
	}
	/* call event specific handling function*/
	if(event->evs_publ_handl) {
		if(event->evs_publ_handl(msg) < 0) {
			LM_ERR("in event specific publish handling\n");
			goto error;
		}
	}

	/* now we have all the necessary values */
	/* fill in the fields of the structure */

	presentity = new_presentity(
			&pres_domain, &pres_user, lexpire, event, &etag, sender);
	if(presentity == NULL) {
		LM_ERR("creating presentity structure\n");
		goto error;
	}

	/* query the database and update or insert */
	if(update_presentity(msg, presentity, &body, etag_gen, &sent_reply,
			sphere, NULL, NULL, 0) < 0) {
		LM_ERR("when updating presentity\n");
		goto error;
	}

	if(presentity) {
		pkg_free(presentity);
	}
	if(etag.s) {
		pkg_free(etag.s);
	}
	if(sender) {
		pkg_free(sender);
	}
	if(sphere) {
		pkg_free(sphere);
	}

	return 1;

unsupported_event:

	LM_WARN("Missing or unsupported event header field value\n");

	if(msg->event && msg->event->body.s && msg->event->body.len > 0) {
		LM_ERR("    event=[%.*s]\n", msg->event->body.len, msg->event->body.s);
	}

	reply_code = BAD_EVENT_CODE;
	reply_str = pu_489_rpl;

error:
	if(sent_reply == 0) {
		if(send_error_reply(msg, reply_code, reply_str) < 0) {
			LM_ERR("failed to send error reply\n");
		}
	}

	if(presentity) {
		pkg_free(presentity);
	}
	if(etag.s) {
		pkg_free(etag.s);
	}
	if(sender) {
		pkg_free(sender);
	}
	if(sphere) {
		pkg_free(sphere);
	}

	return -1;
}

int ki_handle_publish(struct sip_msg *msg)
{
	return ki_handle_publish_uri(msg, NULL);
}

/**
 * PUBLISH request handling
 *
 */
int w_handle_publish(struct sip_msg *msg, char *sender_uri, char *str2)
{
	str suri;

	if(sender_uri != NULL
			&& fixup_get_svalue(msg, (gparam_t *)sender_uri, &suri) != 0) {
		LM_ERR("invalid uri parameter\n");
		return -1;
	}

	return ki_handle_publish_uri(msg, (sender_uri) ? &suri : NULL);
}

int update_hard_presentity(
		str *pres_uri, pres_ev_t *event, str *file_uri, str *filename)
{
	int ret = -1, new_t, pidf_result;
	str *pidf_doc = 0;
	char *sphere = NULL;
	presentity_t *pres = NULL;
	struct sip_uri parsed_uri;

	LM_INFO("Hard-state file %.*s (uri %.*s) updated for %.*s\n", filename->len,
			filename->s, file_uri->len, file_uri->s, pres_uri->len,
			pres_uri->s);

	if(!event->get_pidf_doc) {
		LM_WARN("pidf-manipulation not supported for %.*s\n", event->name.len,
				event->name.s);
		return -1;
	}

	if(parse_uri(pres_uri->s, pres_uri->len, &parsed_uri) < 0) {
		LM_ERR("bad presentity URI\n");
		return -1;
	}

	pidf_result = event->get_pidf_doc(
			&parsed_uri.user, &parsed_uri.host, file_uri, &pidf_doc);

	if(pidf_result < 0) {
		LM_ERR("retrieving pidf-manipulation document\n");
		return -1;
	} else if(pidf_result > 0) {
		/* Insert/replace presentity... */
		LM_DBG("INSERT/REPLACE\n");
		xmlDocPtr doc;

		if(pres_sphere_enable) {
			sphere = extract_sphere(pidf_doc);
		}

		doc = xmlParseMemory(pidf_doc->s, pidf_doc->len);
		if(doc == NULL) {
			LM_ERR("bad body format\n");
			xmlFreeDoc(doc);
			xmlCleanupParser();
			xmlMemoryDump();
			goto done;
		}
		xmlFreeDoc(doc);
		xmlCleanupParser();
		xmlMemoryDump();

		new_t = 1;
	} else {
		/* Delete presentity... */
		LM_DBG("DELETE\n");
		new_t = 0;
	}

	pres = new_presentity(
			&parsed_uri.host, &parsed_uri.user, -1, event, filename, NULL);
	if(pres == NULL) {
		LM_ERR("creating presentity structure\n");
		goto done;
	}

	if(update_presentity(
			   NULL, pres, pidf_doc, new_t, NULL, sphere, NULL, NULL, 0)
			< 0) {
		LM_ERR("updating presentity\n");
		goto done;
	}

	ret = 1;

done:
	if(pres)
		pkg_free(pres);
	if(sphere)
		pkg_free(sphere);
	if(pidf_doc) {
		if(pidf_doc->s)
			pkg_free(pidf_doc->s);
		pkg_free(pidf_doc);
	}

	return ret;
}