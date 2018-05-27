/*
 * rls module - resource list server
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../core/ut.h"
#include "../../core/dprint.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/strutils.h"
#include "../../core/hashes.h"
#include "../../core/parser/parse_supported.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_event.h"
#include "../../core/parser/parse_expires.h"
#include "../../core/parser/parse_cseq.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_rr.h"
#include "../presence/subscribe.h"
#include "../presence/utils_func.h"
#include "../presence/hash.h"
#include "subscribe.h"
#include "notify.h"
#include "rls.h"
#include "../../core/mod_fix.h"
#include "list.h"
#include "utils.h"

int counter= 0;

static str su_200_rpl     = str_init("OK");
static str pu_421_rpl     = str_init("Extension Required");
static str pu_400_rpl     = str_init("Bad request");
static str stale_cseq_rpl = str_init("Stale Cseq Value");
static str pu_489_rpl     = str_init("Bad Event");

#define Stale_cseq_code 401

subs_t* constr_new_subs(struct sip_msg* msg, struct to_body *pto,
		pres_ev_t* event);
int resource_subscriptions(subs_t* subs, xmlNodePtr rl_node);

int update_rlsubs( subs_t* subs,unsigned int hash_code);
int remove_expired_rlsubs( subs_t* subs,unsigned int hash_code);

/**
 * return the XML node for rls-services matching uri
 */
xmlNodePtr rls_get_by_service_uri(xmlDocPtr doc, str* service_uri)
{
	xmlNodePtr root, node;
	struct sip_uri sip_uri;
	str uri, uri_str;
	str *normalized_uri;

	root = XMLDocGetNodeByName(doc, "rls-services", NULL);
	if(root==NULL)
	{
		LM_ERR("no rls-services node in XML document\n");
		return NULL;
	}

	for(node=root->children; node; node=node->next)
	{
		if(xmlStrcasecmp(node->name, (unsigned char*)"service")==0)
		{
			uri.s = XMLNodeGetAttrContentByName(node, "uri");
			if (uri.s == NULL)
			{
				LM_DBG("failed to fetch 'uri' in service [invalid XML from XCAP]\n");
				continue;
			}
			uri.len = strlen(uri.s);
			normalized_uri = normalize_sip_uri(&uri);
			if (normalized_uri->s == NULL || normalized_uri->len == 0)
			{
				LM_ERR("failed to normalize service URI\n");
				xmlFree(uri.s);
				return NULL;
			}
			xmlFree(uri.s);
			if(parse_uri(normalized_uri->s, normalized_uri->len, &sip_uri)< 0)
			{
				LM_ERR("failed to parse uri\n");
				return NULL;
			}
			if(uandd_to_uri(sip_uri.user, sip_uri.host, &uri_str)< 0)
			{
				LM_ERR("failed to construct uri from user and domain\n");
				return NULL;
			}
			if(uri_str.len== service_uri->len &&
					strncmp(uri_str.s, service_uri->s, uri_str.len) == 0)
			{
				pkg_free(uri_str.s);
				return node;
			}
			LM_DBG("match not found, service-uri = [%.*s]\n", uri_str.len, uri_str.s);
			pkg_free(uri_str.s);
		}
	}
	return NULL;
}

/**
 * return service_node and rootdoc based on service uri, user and domain
 * - document is taken from DB or via xcap client
 */
int rls_get_service_list(str *service_uri, str *user, str *domain,
		xmlNodePtr *service_node, xmlDocPtr *rootdoc)
{
	db_key_t query_cols[5];
	db_val_t query_vals[5];
	db_key_t result_cols[3];
	int n_query_cols = 0;
	db1_res_t *result = 0;
	db_row_t *row;
	db_val_t *row_vals;
	str body;
	int n_result_cols= 0;
	int etag_col, xcap_col;
	char *etag= NULL;
	xcap_get_req_t req;
	xmlDocPtr xmldoc = NULL;
	xcap_doc_sel_t doc_sel;
	char *xcapdoc= NULL;

	if(service_uri==NULL || user==NULL || domain==NULL)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	/* first search in database */
	query_cols[n_query_cols] = &str_username_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *user;
	n_query_cols++;

	query_cols[n_query_cols] = &str_domain_col;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = *domain;
	n_query_cols++;

	query_cols[n_query_cols] = &str_doc_type_col;
	query_vals[n_query_cols].type = DB1_INT;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.int_val= RLS_SERVICE;
	n_query_cols++;

	LM_DBG("searching document for user sip:%.*s@%.*s\n",
			user->len, user->s, domain->len, domain->s);

	if(rls_xcap_dbf.use_table(rls_xcap_db, &rls_xcap_table) < 0)
	{
		LM_ERR("in use_table-[table]= %.*s\n",
				rls_xcap_table.len, rls_xcap_table.s);
		return -1;
	}

	result_cols[xcap_col= n_result_cols++] = &str_doc_col;
	result_cols[etag_col= n_result_cols++] = &str_etag_col;

	if(rls_xcap_dbf.query(rls_xcap_db, query_cols, 0 , query_vals, result_cols,
				n_query_cols, n_result_cols, 0, &result)<0)
	{
		LM_ERR("failed querying table xcap for document [service_uri]=%.*s\n",
				service_uri->len, service_uri->s);
		if(result)
			rls_xcap_dbf.free_result(rls_xcap_db, result);
		return -1;
	}

	if (result == NULL)
	{
		LM_ERR("bad result\n");
		return -1;
	}

	if(result->n<=0)
	{
		LM_DBG("No rl document found\n");

		if(rls_integrated_xcap_server)
		{
			rls_xcap_dbf.free_result(rls_xcap_db, result);
			return 0;
		}

		/* make an initial request to xcap_client module */
		memset(&doc_sel, 0, sizeof(xcap_doc_sel_t));
		doc_sel.auid.s= "rls-services";
		doc_sel.auid.len= strlen("rls-services");
		doc_sel.doc_type= RLS_SERVICE;
		doc_sel.type= USERS_TYPE;
		if(uandd_to_uri(*user, *domain, &doc_sel.xid)<0)
		{
			LM_ERR("failed to create uri from user and domain\n");
			goto error;
		}

		memset(&req, 0, sizeof(xcap_get_req_t));
		req.xcap_root= xcap_root;
		req.port= xcap_port;
		req.doc_sel= doc_sel;
		req.etag= etag;
		req.match_type= IF_NONE_MATCH;

		xcapdoc= xcap_GetNewDoc(req, *user, *domain);
		if(xcapdoc==NULL)
		{
			LM_ERR("while fetching data from xcap server\n");
			pkg_free(doc_sel.xid.s);
			goto error;
		}
		pkg_free(doc_sel.xid.s);
		body.s = xcapdoc;
		body.len = strlen(xcapdoc);
	} else {
		row = &result->rows[0];
		row_vals = ROW_VALUES(row);

		body.s = (char*)row_vals[xcap_col].val.string_val;
		if(body.s== NULL)
		{
			LM_ERR("xcap doc is null\n");
			goto error;
		}
		body.len = strlen(body.s);
		if(body.len==0)
		{
			LM_ERR("xcap document is empty\n");
			goto error;
		}
	}

	LM_DBG("rls_services document:\n%.*s", body.len, body.s);
	xmldoc = xmlParseMemory(body.s, body.len);
	if(xmldoc==NULL)
	{
		LM_ERR("while parsing XML memory\n");
		goto error;
	}

	*service_node = rls_get_by_service_uri(xmldoc, service_uri);
	if(*service_node==NULL)
	{
		LM_DBG("service uri %.*s not found in rl document for user"
				" sip:%.*s@%.*s\n", service_uri->len, service_uri->s,
				user->len, user->s, domain->len, domain->s);
		if(xmldoc!=NULL)
			xmlFreeDoc(xmldoc);
		*rootdoc = NULL;
	} else {
		*rootdoc = xmldoc;
	}

	rls_xcap_dbf.free_result(rls_xcap_db, result);
	if(xcapdoc!=NULL)
		pkg_free(xcapdoc);

	return 0;

error:
	if(result!=NULL)
		rls_xcap_dbf.free_result(rls_xcap_db, result);
	if(xcapdoc!=NULL)
		pkg_free(xcapdoc);

	return -1;
}

/**
 * reply 421 with require header
 */
int reply_421(struct sip_msg* msg)
{
	str hdr_append;
	char buffer[256];

	hdr_append.s = buffer;
	hdr_append.s[0]='\0';
	hdr_append.len = sprintf(hdr_append.s, "Require: eventlist\r\n");
	if(hdr_append.len < 0)
	{
		LM_ERR("unsuccessful sprintf\n");
		return -1;
	}
	hdr_append.s[hdr_append.len]= '\0';

	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		return -1;
	}

	if (slb.freply(msg, 421, &pu_421_rpl) < 0)
	{
		LM_ERR("while sending reply\n");
		return -1;
	}
	return 0;

}

/**
 * reply 200 ok with require header
 */
int reply_200(struct sip_msg* msg, str* contact, int expires)
{
	str hdr_append;

	hdr_append.s = (char *)pkg_malloc( sizeof(char)*(contact->len+70));
	if(hdr_append.s == NULL)
	{
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	hdr_append.len = snprintf(hdr_append.s, contact->len+70,
				"Expires: %d" CRLF
				"Contact: <%.*s>" CRLF
				"Require: eventlist" CRLF,
				expires,
				contact->len, contact->s
			);
	if(hdr_append.len<0 || hdr_append.len>=contact->len+70)
	{
		LM_ERR("unsuccessful snprintf\n");
		goto error;
	}

	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		goto error;
	}

	if(slb.freply(msg, 200, &su_200_rpl) < 0)
	{
		LM_ERR("while sending reply\n");
		goto error;
	}
	pkg_free(hdr_append.s);
	return 0;

error:
	pkg_free(hdr_append.s);
	return -1;
}

/**
 * reply 489 with allow-events header
 */
int reply_489(struct sip_msg * msg)
{
	str hdr_append;
	char buffer[256];
	str* ev_list;

	hdr_append.s = buffer;
	hdr_append.s[0]='\0';
	hdr_append.len = sprintf(hdr_append.s, "Allow-Events: ");
	if(hdr_append.len < 0)
	{
		LM_ERR("unsuccessful sprintf\n");
		return -1;
	}

	if(pres_get_ev_list(&ev_list)< 0)
	{
		LM_ERR("while getting ev_list\n");
		return -1;
	}
	memcpy(hdr_append.s+ hdr_append.len, ev_list->s, ev_list->len);
	hdr_append.len+= ev_list->len;
	pkg_free(ev_list->s);
	pkg_free(ev_list);
	memcpy(hdr_append.s+ hdr_append.len, CRLF, CRLF_LEN);
	hdr_append.len+=  CRLF_LEN;
	hdr_append.s[hdr_append.len]= '\0';

	if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		return -1;
	}
	if (slb.freply(msg, 489, &pu_489_rpl) < 0)
	{
		LM_ERR("while sending reply\n");
		return -1;
	}
	return 0;
}


/**
 * handle RLS subscription
 */
int ki_rls_handle_subscribe(struct sip_msg* msg)
{
	struct to_body *pfrom;

	if (parse_from_uri(msg) == NULL)
	{
		LM_ERR("failed to find From header\n");
		if (slb.freply(msg, 400, &pu_400_rpl) < 0)
		{
			LM_ERR("while sending 400 reply\n");
			return -1;
		}
		return 0;
	}
	pfrom = (struct to_body *) msg->from->parsed;

	return rls_handle_subscribe(msg, pfrom->parsed_uri.user,
			pfrom->parsed_uri.host);
}

/**
 * handle RLS subscription
 */
int w_rls_handle_subscribe0(struct sip_msg* msg, char *p1, char *p2)
{
	return ki_rls_handle_subscribe(msg);
}

int ki_rls_handle_subscribe_uri(sip_msg_t* msg, str *wuri)
{
	struct sip_uri parsed_wuri;

	if (parse_uri(wuri->s, wuri->len, &parsed_wuri) < 0)
	{
		LM_ERR("failed to parse watcher URI\n");
		return -1;
	}

	return rls_handle_subscribe(msg, parsed_wuri.user, parsed_wuri.host);
}

int w_rls_handle_subscribe1(struct sip_msg* msg, char* watcher_uri, char *p2)
{
	str wuri;

	if (fixup_get_svalue(msg, (gparam_p)watcher_uri, &wuri) != 0)
	{
		LM_ERR("invalid uri parameter\n");
		return -1;
	}
	return ki_rls_handle_subscribe_uri(msg, &wuri);
}

int rls_handle_subscribe(struct sip_msg* msg, str watcher_user, str watcher_domain)
{
	subs_t subs;
	pres_ev_t* event = NULL;
	int err_ret = -1;
	int ret = to_presence_code;
	xmlDocPtr doc = NULL;
	xmlNodePtr service_node = NULL;
	unsigned int hash_code=0;
	int to_tag_gen = 0;
	event_t* parsed_event;
	param_t* ev_param = NULL;
	str reason;
	int rt;
	str rlsubs_did = {0, 0};

	memset(&subs, 0, sizeof(subs_t));

	/** sanity checks - parse all headers */
	if (parse_headers(msg, HDR_EOH_F, 0)<-1)
	{
		LM_ERR("failed parsing all headers\n");
		if (slb.freply(msg, 400, &pu_400_rpl) < 0)
		{
			LM_ERR("while sending 400 reply\n");
			return -1;
		}
		return 0;
	}
	/* check for To and From headesr */
	if(parse_to_uri(msg)==NULL || parse_from_uri(msg)==NULL)
	{
		LM_ERR("failed to find To or From headers\n");
		if (slb.freply(msg, 400, &pu_400_rpl) < 0)
		{
			LM_ERR("while sending 400 reply\n");
			return -1;
		}
		return 0;
	}
	if(get_from(msg)->tag_value.s ==NULL || get_from(msg)->tag_value.len==0)
	{
		LM_ERR("no from tag value present\n");
		return -1;
	}
	if(msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LM_ERR("cannot find callid header\n");
		return -1;
	}
	if(parse_sip_msg_uri(msg)<0)
	{
		LM_ERR("failed parsing Request URI\n");
		return -1;
	}

	/* check for header 'Support: eventlist' */
	if(msg->supported==NULL)
	{
		LM_DBG("supported header not found - not for rls\n");
		goto forpresence;
	}

	if(parse_supported(msg)<0)
	{
		LM_ERR("failed to parse supported headers\n");
		return -1;
	}

	if(!(get_supported(msg) & F_OPTION_TAG_EVENTLIST))
	{
		LM_DBG("No support for 'eventlist' - not for rls\n");
		goto forpresence;
	}

	/* inspecting the Event header field */
	if(msg->event && msg->event->body.len > 0)
	{
		if (!msg->event->parsed && (parse_event(msg->event) < 0))
		{
			LM_ERR("cannot parse Event header\n");
			goto error;
		}
		if(! ( ((event_t*)msg->event->parsed)->type & rls_events) )
		{
			goto forpresence;
		}
	} else {
		goto bad_event;
	}

	/* search event in the list */
	parsed_event = (event_t*)msg->event->parsed;
	event = pres_search_event(parsed_event);
	if(event==NULL)
	{
		goto bad_event;
	}
	subs.event= event;

	/* extract the id if any*/
	ev_param= parsed_event->params.list;
	while(ev_param)
	{
		if(ev_param->name.len==2 && strncmp(ev_param->name.s, "id", 2)==0)
		{
			subs.event_id = ev_param->body;
			break;
		}
		ev_param= ev_param->next;
	}

	/* extract dialog information from message headers */
	if(pres_extract_sdialog_info(&subs, msg, rls_max_expires,
				&to_tag_gen, rls_server_address,
				watcher_user, watcher_domain)<0)
	{
		LM_ERR("bad subscribe request\n");
		goto error;
	}

	hash_code = core_hash(&subs.callid, &subs.to_tag, hash_size);
	if (CONSTR_RLSUBS_DID(&subs, &rlsubs_did) < 0)
	{
		LM_ERR("cannot build rls subs did\n");
		goto error;
	}
	subs.updated = core_hash(&rlsubs_did, NULL, 0) %
		(waitn_time * rls_notifier_poll_rate * rls_notifier_processes);

	if(get_to(msg)->tag_value.s==NULL || get_to(msg)->tag_value.len==0)
	{ /* initial Subscribe */
		/*verify if Request URI represents a list by asking xcap server*/
		if(uandd_to_uri(msg->parsed_uri.user, msg->parsed_uri.host,
					&subs.pres_uri)<0)
		{
			LM_ERR("while constructing uri from user and domain\n");
			goto error;
		}
		if(rls_get_service_list(&subs.pres_uri, &subs.watcher_user,
					&subs.watcher_domain, &service_node, &doc)<0)
		{
			LM_ERR("while attepmting to get a resource list\n");
			goto error;
		}
		if(doc==NULL)
		{
			/* if not for RLS, pass it to presence serivce */
			LM_DBG("list not found - searched for uri <%.*s>\n",
					subs.pres_uri.len, subs.pres_uri.s);
			goto forpresence;
		}

		/* if correct reply with 200 OK */
		if(reply_200(msg, &subs.local_contact, subs.expires)<0)
			goto error;

		subs.local_cseq = 0;

		if(subs.expires != 0)
		{
			subs.version = 1;
			if (dbmode==RLS_DB_ONLY)
			{
				rt=insert_rlsdb( &subs );
			}
			else
			{
				rt=pres_insert_shtable(rls_table, hash_code, &subs);
			}
			if (rt<0)
			{
				LM_ERR("while adding new subscription\n");
				goto error;
			}
		}
	} else {
		/* search if a stored dialog */
		if ( dbmode == RLS_DB_ONLY )
		{
			if (rls_dbf.start_transaction)
			{
				if (rls_dbf.start_transaction(rls_db, DB_LOCKING_WRITE) < 0)
				{
					LM_ERR("in start_transaction\n");
					goto error;
				}
			}

			rt = get_dialog_subscribe_rlsdb(&subs);

			if (rt <= 0)
			{
				LM_DBG("subscription dialog not found for <%.*s@%.*s>\n",
						subs.watcher_user.len, subs.watcher_user.s,
						subs.watcher_domain.len, subs.watcher_domain.s);

				if (rls_dbf.end_transaction)
				{
					if (rls_dbf.end_transaction(rls_db) < 0)
					{
						LM_ERR("in end_transaction\n");
						goto error;
					}
				}

				goto forpresence;
			}
			else if(rt>=400)
			{
				reason = (rt==400)?pu_400_rpl:stale_cseq_rpl;

				if (slb.freply(msg, 400, &reason) < 0)
				{
					LM_ERR("while sending reply\n");
					goto error;
				}

				if (rls_dbf.end_transaction)
				{
					if (rls_dbf.end_transaction(rls_db) < 0)
					{
						LM_ERR("in end_transaction\n");
						goto error;
					}
				}

				ret = 0;
				goto stop;
			}

			/* if correct reply with 200 OK */
			if(reply_200(msg, &subs.local_contact, subs.expires)<0)
				goto error;

			if (update_dialog_subscribe_rlsdb(&subs) < 0)
			{
				LM_ERR("while updating resource list subscription\n");
				goto error;
			}

			if (rls_dbf.end_transaction)
			{
				if (rls_dbf.end_transaction(rls_db) < 0)
				{
					LM_ERR("in end_transaction\n");
					goto error;
				}
			}
		}
		else
		{
			lock_get(&rls_table[hash_code].lock);
			if(pres_search_shtable(rls_table, subs.callid,
						subs.to_tag, subs.from_tag, hash_code)==NULL)
			{
				lock_release(&rls_table[hash_code].lock);
				LM_DBG("subscription dialog not found for <%.*s@%.*s>\n",
						subs.watcher_user.len, subs.watcher_user.s,
						subs.watcher_domain.len, subs.watcher_domain.s);
				goto forpresence;
			}
			lock_release(&rls_table[hash_code].lock);

			/* if correct reply with 200 OK */
			if(reply_200(msg, &subs.local_contact, subs.expires)<0)
				goto error;

			rt = update_rlsubs(&subs, hash_code);

			if(rt<0)
			{
				LM_ERR("while updating resource list subscription\n");
				goto error;
			}

			if(rt>=400)
			{
				reason = (rt==400)?pu_400_rpl:stale_cseq_rpl;

				if (slb.freply(msg, 400, &reason) < 0)
				{
					LM_ERR("while sending reply\n");
					goto error;
				}
				ret = 0;
				goto stop;
			}
		}
		if(rls_get_service_list(&subs.pres_uri, &subs.watcher_user,
					&subs.watcher_domain, &service_node, &doc)<0)
		{
			LM_ERR("failed getting resource list\n");
			goto error;
		}
		if(doc==NULL)
		{
			/* warning: no document returned?!?! */
			LM_WARN("no document returned for uri <%.*s>\n",
					subs.pres_uri.len, subs.pres_uri.s);
			goto done;
		}
	}

	if (get_to(msg)->tag_value.s==NULL || get_to(msg)->tag_value.len==0)
	{
		/* initial subscriber - sending notify with full state */
		if(send_full_notify(&subs, service_node, &subs.pres_uri, hash_code)<0)
		{
			LM_ERR("failed sending full state notify\n");
			goto error;
		}
	}

	/* send subscribe requests for all in the list */
	if(resource_subscriptions(&subs, service_node)< 0)
	{
		LM_ERR("failed sending subscribe requests to resources in list\n");
		goto error;
	}

	if (dbmode !=RLS_DB_ONLY)
		remove_expired_rlsubs(&subs, hash_code);

done:
	ret = 1;
stop:
forpresence:
	if(subs.pres_uri.s!=NULL)
		pkg_free(subs.pres_uri.s);
	if(subs.record_route.s!=NULL)
		pkg_free(subs.record_route.s);
	if(doc!=NULL)
		xmlFreeDoc(doc);
	if (rlsubs_did.s != NULL)
		pkg_free(rlsubs_did.s);
	return ret;

bad_event:
	err_ret = 0;
	if(reply_489(msg)<0)
	{
		LM_ERR("failed sending 489 reply\n");
		err_ret = -1;
	}

error:
	LM_ERR("occurred in rls_handle_subscribe\n");
	if(subs.pres_uri.s!=NULL)
		pkg_free(subs.pres_uri.s);

	if(subs.record_route.s!=NULL)
		pkg_free(subs.record_route.s);

	if(doc!=NULL)
		xmlFreeDoc(doc);

	if (rlsubs_did.s != NULL)
		pkg_free(rlsubs_did.s);

	if (rls_dbf.abort_transaction)
	{
		if (rls_dbf.abort_transaction(rls_db) < 0)
			LM_ERR("in abort_transaction\n");
	}

	return err_ret;
}

int remove_expired_rlsubs( subs_t* subs, unsigned int hash_code)
{
	subs_t* s, *ps;
	int found= 0;

	if(subs->expires!=0)
		return 0;

	if (dbmode == RLS_DB_ONLY)
	{
		LM_ERR( "remove_expired_rlsubs called in RLS_DB_ONLY mode\n" );
	}

	/* search the record in hash table */
	lock_get(&rls_table[hash_code].lock);

	s= pres_search_shtable(rls_table, subs->callid,
			subs->to_tag, subs->from_tag, hash_code);
	if(s== NULL)
	{
		LM_DBG("record not found in hash table\n");
		lock_release(&rls_table[hash_code].lock);
		return -1;
	}
	/* delete record from hash table */
	ps= rls_table[hash_code].entries;
	while(ps->next)
	{
		if(ps->next== s)
		{
			found= 1;
			break;
		}
		ps= ps->next;
	}
	if(found== 0)
	{
		LM_ERR("record not found\n");
		lock_release(&rls_table[hash_code].lock);
		return -1;
	}
	ps->next= s->next;
	shm_free(s);

	lock_release(&rls_table[hash_code].lock);

	return 0;

}

int update_rlsubs( subs_t* subs, unsigned int hash_code)
{
	subs_t* s;

	if (dbmode == RLS_DB_ONLY)
	{
		LM_ERR( "update_rlsubs called in RLS_DB_ONLY mode\n" );
	}

	/* search the record in hash table */
	lock_get(&rls_table[hash_code].lock);

	s= pres_search_shtable(rls_table, subs->callid,
			subs->to_tag, subs->from_tag, hash_code);
	if(s== NULL)
	{
		LM_DBG("record not found in hash table\n");
		lock_release(&rls_table[hash_code].lock);
		return -1;
	}

	s->expires= subs->expires+ (int)time(NULL);

	if(s->db_flag & NO_UPDATEDB_FLAG)
		s->db_flag= UPDATEDB_FLAG;

	if(	s->remote_cseq>= subs->remote_cseq)
	{
		lock_release(&rls_table[hash_code].lock);
		LM_DBG("stored cseq= %d\n", s->remote_cseq);
		return Stale_cseq_code;
	}

	s->remote_cseq= subs->remote_cseq;

	subs->pres_uri.s= (char*)pkg_malloc(s->pres_uri.len* sizeof(char));
	if(subs->pres_uri.s== NULL)
	{
		ERR_MEM(PKG_MEM_STR);
	}
	memcpy(subs->pres_uri.s, s->pres_uri.s, s->pres_uri.len);
	subs->pres_uri.len= s->pres_uri.len;

	if(s->record_route.s!=NULL && s->record_route.len>0)
	{
		subs->record_route.s =
			(char*)pkg_malloc(s->record_route.len* sizeof(char));
		if(subs->record_route.s==NULL)
		{
			ERR_MEM(PKG_MEM_STR);
		}
		memcpy(subs->record_route.s, s->record_route.s, s->record_route.len);
		subs->record_route.len= s->record_route.len;
	}

	subs->local_cseq= s->local_cseq;
	subs->version= s->version;

	lock_release(&rls_table[hash_code].lock);

	return 0;

error:
	lock_release(&rls_table[hash_code].lock);
	return -1;
}

int send_resource_subs(char* uri, void* param)
{
	str pres_uri, *tmp_str;
	struct sip_uri parsed_pres_uri;
	int duplicate = 0;
	str *normalized_uri;

	subs_info_t *s = (subs_info_t *) ((void**)param)[0];
	list_entry_t **rls_contact_list = (list_entry_t **) ((void**)param)[1];

	pres_uri.s = uri;
	pres_uri.len = strlen(uri);

	normalized_uri = normalize_sip_uri(&pres_uri);
	if (normalized_uri->s == NULL || normalized_uri->len == 0) {
		LM_ERR("failed to normalize RLS entry URI %.*s\n",
				pres_uri.len, pres_uri.s);
		return -1;
	}
	LM_DBG("pres uri [%.*s] - normalized uri [%.*s]\n",
			pres_uri.len, pres_uri.s, normalized_uri->len, normalized_uri->s);

	if (parse_uri(normalized_uri->s, normalized_uri->len, &parsed_pres_uri)
			< 0) {
		LM_ERR("bad uri: %.*s\n", normalized_uri->len, normalized_uri->s);
		return -1;
	}

	if (check_self(&parsed_pres_uri.host, 0, PROTO_NONE) != 1
			&& rls_disable_remote_presence != 0)
	{
		LM_WARN("Unable to subscribe to remote contact %.*s for watcher %.*s\n",
				normalized_uri->len, normalized_uri->s,
				s->watcher_uri->len,
				s->watcher_uri->s);
		return 1;
	}

	/* Silently drop subscribes over the limit - will print a single warning
	 * later */
	if (rls_max_backend_subs > 0 && ++counter > rls_max_backend_subs)
		return 1;

	s->pres_uri = normalized_uri;
	s->remote_target = normalized_uri;

	/* Build list of contacts... checking each contact exists only once */
	if ((tmp_str = (str *)pkg_malloc(sizeof(str))) == NULL)
	{
		LM_ERR("out of private memory\n");
		return -1;
	}
	if ((tmp_str->s = (char *)pkg_malloc(sizeof(char)
							* normalized_uri->len)) == NULL)
	{
		pkg_free(tmp_str);
		LM_ERR("out of private memory\n");
		return -1;
	}
	memcpy(tmp_str->s, normalized_uri->s, normalized_uri->len);
	tmp_str->len = normalized_uri->len;
	*rls_contact_list = list_insert(tmp_str, *rls_contact_list, &duplicate);
	if (duplicate != 0)
	{
		LM_WARN("%.*s has %.*s multiple times in the same resource list\n",
				s->watcher_uri->len, s->watcher_uri->s,
				s->pres_uri->len, s->pres_uri->s);
		pkg_free(tmp_str->s);
		pkg_free(tmp_str);
		return 1;
	}

	return pua_send_subscribe(s);
}

/**
 * send subscriptions to the list from XML node
 */
int resource_subscriptions(subs_t* subs, xmlNodePtr xmlnode)
{
	subs_info_t s;
	str wuri= {0, 0};
	str extra_headers;
	str did_str= {0, 0};
	str *tmp_str;
	list_entry_t *rls_contact_list = NULL;
	list_entry_t *rls_subs_list = NULL;
	void* params[2] = {&s, &rls_contact_list};

	/* if is initial send an initial Subscribe
	 * else search in hash table for a previous subscription */

	if(CONSTR_RLSUBS_DID(subs, &did_str)<0)
	{
		LM_ERR("cannot build rls subs did\n");
		goto error;
	}

	memset(&s, 0, sizeof(subs_info_t));

	if(uandd_to_uri(subs->watcher_user, subs->watcher_domain, &wuri)<0)
	{
		LM_ERR("while constructing uri from user and domain\n");
		goto error;
	}
	s.id = did_str;
	s.watcher_uri = &wuri;
	s.contact = &rls_server_address;
	s.event = get_event_flag(&subs->event->name);
	if(s.event<0)
	{
		LM_ERR("not recognized event\n");
		goto error;
	}
	s.expires = subs->expires;
	s.source_flag = RLS_SUBSCRIBE;
	if(rls_outbound_proxy.s)
		s.outbound_proxy = &rls_outbound_proxy;
	extra_headers.s = "Supported: eventlist\r\n"
		"Accept: application/pidf+xml, application/rlmi+xml,"
		" application/watcherinfo+xml,"
		" multipart/related\r\n";
	extra_headers.len = strlen(extra_headers.s);

	s.extra_headers = &extra_headers;

	s.internal_update_flag = subs->internal_update_flag;

	counter = 0;

	if(process_list_and_exec(xmlnode, subs->watcher_user, subs->watcher_domain,
				send_resource_subs, params)<0)
	{
		LM_ERR("while processing list\n");
		goto error;
	}

	if (rls_max_backend_subs > 0 && counter > rls_max_backend_subs)
		LM_WARN("%.*s has too many contacts.  Max: %d, has: %d\n",
				wuri.len, wuri.s, rls_max_backend_subs, counter);

	if (s.internal_update_flag == INTERNAL_UPDATE_TRUE)
	{
		counter = 0;
		s.internal_update_flag = 0;

		rls_subs_list = pua_get_subs_list(&did_str);

		while ((tmp_str = list_pop(&rls_contact_list)) != NULL)
		{
			LM_DBG("Finding and removing %.*s from subscription list\n", tmp_str->len, tmp_str->s);
			rls_subs_list = list_remove(*tmp_str, rls_subs_list);
			pkg_free(tmp_str->s);
			pkg_free(tmp_str);
		}

		while ((tmp_str = list_pop(&rls_subs_list)) != NULL)
		{
			LM_DBG("Removing subscription for %.*s\n", tmp_str->len, tmp_str->s);
			s.expires = 0;
			send_resource_subs(tmp_str->s, params);
			pkg_free(tmp_str->s);
			pkg_free(tmp_str);
		}
	}
	if (rls_contact_list != NULL)
		list_free(&rls_contact_list);

	pkg_free(wuri.s);
	pkg_free(did_str.s);

	return 0;

error:
	if(wuri.s)
		pkg_free(wuri.s);
	if(did_str.s)
		pkg_free(did_str.s);
	if(rls_contact_list)
		list_free(&rls_contact_list);
	return -1;

}

int fixup_update_subs(void** param, int param_no)
{
	if (param_no == 1) {
		return fixup_spve_null(param, 1);
	} else if (param_no == 2) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}

void update_a_sub(subs_t *subs_copy)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr service_node = NULL;
	int now = (int)time(NULL);

	if (subs_copy->expires < now)
	{
		subs_copy->expires = 0;
		LM_WARN("found expired subscription for: %.*s\n",
				subs_copy->pres_uri.len, subs_copy->pres_uri.s);
		goto done;
	}
	subs_copy->expires -= now;

	if(rls_get_service_list(&subs_copy->pres_uri, &subs_copy->watcher_user,
				&subs_copy->watcher_domain, &service_node, &doc)<0)
	{
		LM_ERR("failed getting resource list for: %.*s\n",
				subs_copy->pres_uri.len, subs_copy->pres_uri.s);
		goto done;
	}

	if(doc==NULL)
	{
		LM_WARN("no document returned for: %.*s\n",
				subs_copy->pres_uri.len, subs_copy->pres_uri.s);
		goto done;
	}

	subs_copy->internal_update_flag = INTERNAL_UPDATE_TRUE;

	if(resource_subscriptions(subs_copy, service_node)< 0)
	{
		LM_ERR("failed sending subscribe requests to resources in list\n");
		goto done;
	}

done:
	if (doc != NULL)
		xmlFreeDoc(doc);

	pkg_free(subs_copy);
}

int ki_rls_update_subs(struct sip_msg *msg, str *uri, str *event)
{
	struct sip_uri parsed_uri;
	event_t e;
	int i;

	if (event_parser(event->s, event->len, &e) < 0)
	{
		LM_ERR("while parsing event: %.*s\n", event->len, event->s);
		return -1;
	}

	if (e.type & EVENT_OTHER)
	{
		LM_ERR("unrecognised event: %.*s\n", event->len, event->s);
		return -1;
	}

	if (!(e.type & rls_events))
	{
		LM_ERR("event not supported by RLS: %.*s\n", event->len,
				event->s);
		return -1;
	}

	if (parse_uri(uri->s, uri->len, &parsed_uri) < 0)
	{
		LM_ERR("bad uri: %.*s\n", uri->len, uri->s);
		return -1;
	}

	LM_DBG("watcher username: %.*s, watcher domain: %.*s\n",
			parsed_uri.user.len, parsed_uri.user.s,
			parsed_uri.host.len, parsed_uri.host.s);

	if (dbmode==RLS_DB_ONLY)
	{
		int ret;
		lock_get(rls_update_subs_lock);
		ret = (update_all_subs_rlsdb(&parsed_uri.user, &parsed_uri.host, event));
		lock_release(rls_update_subs_lock);
		return ret;
	}


	if (rls_table == NULL)
	{
		LM_ERR("rls_table is NULL\n");
		return -1;
	}

	/* Search through the entire subscription table for matches... */
	for (i = 0; i < hash_size; i++)
	{
		subs_t *subs;

		lock_get(&rls_table[i].lock);

		subs = rls_table[i].entries->next;

		while (subs != NULL)
		{
			if (subs->watcher_user.len == parsed_uri.user.len &&
					strncmp(subs->watcher_user.s, parsed_uri.user.s, parsed_uri.user.len) == 0 &&
					subs->watcher_domain.len == parsed_uri.host.len &&
					strncmp(subs->watcher_domain.s, parsed_uri.host.s, parsed_uri.host.len) == 0 &&
					subs->event->evp->type == e.type)
			{
				subs_t *subs_copy = NULL;

				LM_DBG("found matching RLS subscription for: %.*s\n",
						subs->pres_uri.len, subs->pres_uri.s);

				if ((subs_copy = pres_copy_subs(subs, PKG_MEM_TYPE)) == NULL)
				{
					LM_ERR("subs_t copy failed\n");
					lock_release(&rls_table[i].lock);
					return -1;
				}

				update_a_sub(subs_copy);
			}
			subs = subs->next;
		}
		lock_release(&rls_table[i].lock);
	}

	return 1;
}

int w_rls_update_subs(struct sip_msg *msg, char *puri, char *pevent)
{
	str uri;
	str event;

	if (fixup_get_svalue(msg, (gparam_p)puri, &uri) != 0)
	{
		LM_ERR("invalid uri parameter\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)pevent, &event) != 0)
	{
		LM_ERR("invalid event parameter\n");
		return -1;
	}

	return ki_rls_update_subs(msg, &uri, &event);
}
