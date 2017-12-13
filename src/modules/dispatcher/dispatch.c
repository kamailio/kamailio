/*
 * dispatcher module
 *
 * Copyright (C) 2004-2006 FhG Fokus
 * Copyright (C) 2005 Voice-System.ro
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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
 */

/*! \file
 * \ingroup dispatcher
 * \brief Dispatcher :: Dispatch
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#include "../../core/ut.h"
#include "../../core/trim.h"
#include "../../core/dprint.h"
#include "../../core/action.h"
#include "../../core/route.h"
#include "../../core/dset.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_param.h"
#include "../../core/usr_avp.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/resolve.h"
#include "../../core/lvalue.h"
#include "../../modules/tm/tm_load.h"
#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_res.h"
#include "../../core/str.h"
#include "../../core/script_cb.h"
#include "../../core/kemi.h"
#include "../../core/fmsg.h"

#include "ds_ht.h"
#include "api.h"
#include "dispatch.h"

#define DS_TABLE_VERSION 1
#define DS_TABLE_VERSION2 2
#define DS_TABLE_VERSION3 3
#define DS_TABLE_VERSION4 4

#define DS_ALG_HASHCALLID 0
#define DS_ALG_HASHFROMURI 1
#define DS_ALG_HASHTOURI 2
#define DS_ALG_HASHRURI 3
#define DS_ALG_ROUNDROBIN 4
#define DS_ALG_HASHAUTHUSER 5
#define DS_ALG_RANDOM 6
#define DS_ALG_HASHPV 7
#define DS_ALG_SERIAL 8
#define DS_ALG_WEIGHT 9
#define DS_ALG_CALLLOAD 10
#define DS_ALG_RELWEIGHT 11
#define DS_ALG_PARALLEL 12

static int _ds_table_version = DS_TABLE_VERSION;

static ds_ht_t *_dsht_load = NULL;

static int *_ds_ping_active = NULL;

extern int ds_force_dst;
extern str ds_event_callback;
extern int ds_ping_latency_stats;
extern float ds_latency_estimator_alpha;

static db_func_t ds_dbf;
static db1_con_t *ds_db_handle = NULL;

ds_set_t **ds_lists = NULL;

int *ds_list_nr = NULL;
int *crt_idx = NULL;
int *next_idx = NULL;

#define _ds_list (ds_lists[*crt_idx])
#define _ds_list_nr (*ds_list_nr)

static void ds_run_route(struct sip_msg *msg, str *uri, char *route);

void shuffle_uint100array(unsigned int *arr);
int ds_reinit_rweight_on_state_change(
		int old_state, int new_state, ds_set_t *dset);

/**
 *
 */
int ds_ping_active_init(void)
{
	if(_ds_ping_active != NULL)
		return 0;
	_ds_ping_active = (int *)shm_malloc(sizeof(int));
	if(_ds_ping_active == NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	*_ds_ping_active = 1;
	return 0;
}

/**
 *
 */
int ds_ping_active_get(void)
{
	if(_ds_ping_active == NULL)
		return -1;
	return *_ds_ping_active;
}

/**
 *
 */
int ds_ping_active_set(int v)
{
	if(_ds_ping_active == NULL)
		return -1;
	*_ds_ping_active = v;
	return 0;
}

/**
 *
 */
int ds_hash_load_init(unsigned int htsize, int expire, int initexpire)
{
	if(_dsht_load != NULL)
		return 0;
	_dsht_load = ds_ht_init(htsize, expire, initexpire);
	if(_dsht_load == NULL)
		return -1;
	return 0;
}

/**
 *
 */
int ds_hash_load_destroy(void)
{
	if(_dsht_load == NULL)
		return -1;
	ds_ht_destroy(_dsht_load);
	_dsht_load = NULL;
	return 0;
}

/**
 * Recursivly print ds_set
 */
void ds_log_set(ds_set_t *node)
{
	if(!node)
		return;

	int i;

	for(i = 0; i < 2; ++i)
		ds_log_set(node->next[i]);

	for(i = 0; i < node->nr; i++) {
		LM_DBG("dst>> %d %.*s %d %d (%.*s,%d,%d,%d)\n", node->id,
				node->dlist[i].uri.len, node->dlist[i].uri.s,
				node->dlist[i].flags, node->dlist[i].priority,
				node->dlist[i].attrs.duid.len, node->dlist[i].attrs.duid.s,
				node->dlist[i].attrs.maxload, node->dlist[i].attrs.weight,
				node->dlist[i].attrs.rweight);
	}

	return;
}

/**
 *
 */
int ds_log_sets(void)
{
	if(_ds_list == NULL)
		return -1;

	ds_log_set(_ds_list);

	return 0;
}

/**
 *
 */
int init_data(void)
{
	int *p;

	ds_lists = (ds_set_t **)shm_malloc(2 * sizeof(ds_set_t *));
	if(!ds_lists) {
		LM_ERR("Out of memory\n");
		return -1;
	}
	memset(ds_lists, 0, 2 * sizeof(ds_set_t *));


	p = (int *)shm_malloc(3 * sizeof(int));
	if(!p) {
		LM_ERR("Out of memory\n");
		return -1;
	}
	memset(p, 0, 3 * sizeof(int));

	crt_idx = p;
	next_idx = p + 1;
	ds_list_nr = p + 2;
	*crt_idx = *next_idx = 0;

	return 0;
}

/**
 *
 */
int ds_set_attrs(ds_dest_t *dest, str *attrs)
{
	param_t *params_list = NULL;
	param_hooks_t phooks;
	param_t *pit = NULL;
	str param;
	int tmp_rweight = 0;

	if(attrs == NULL || attrs->len <= 0)
		return 0;
	if(attrs->s[attrs->len - 1] == ';')
		attrs->len--;
	/* clone in shm */
	dest->attrs.body.s = (char *)shm_malloc(attrs->len + 1);
	if(dest->attrs.body.s == NULL) {
		LM_ERR("no more shm\n");
		return -1;
	}
	memcpy(dest->attrs.body.s, attrs->s, attrs->len);
	dest->attrs.body.s[attrs->len] = '\0';
	dest->attrs.body.len = attrs->len;

	param = dest->attrs.body;
	if(parse_params(&param, CLASS_ANY, &phooks, &params_list) < 0)
		return -1;
	for(pit = params_list; pit; pit = pit->next) {
		if(pit->name.len == 4 && strncasecmp(pit->name.s, "duid", 4) == 0) {
			dest->attrs.duid = pit->body;
		} else if(pit->name.len == 6
				  && strncasecmp(pit->name.s, "weight", 6) == 0) {
			str2sint(&pit->body, &dest->attrs.weight);
		} else if(pit->name.len == 7
				  && strncasecmp(pit->name.s, "maxload", 7) == 0) {
			str2sint(&pit->body, &dest->attrs.maxload);
		} else if(pit->name.len == 6
				  && strncasecmp(pit->name.s, "socket", 6) == 0) {
			dest->attrs.socket = pit->body;
		} else if(pit->name.len == 7
				  && strncasecmp(pit->name.s, "rweight", 7) == 0) {
			tmp_rweight = 0;
			str2sint(&pit->body, &tmp_rweight);
			if(tmp_rweight >= 1 && tmp_rweight <= 100) {
				dest->attrs.rweight = tmp_rweight;
			} else {
				LM_ERR("rweight %d not in 1-100 range; skipped", tmp_rweight);
			}
		}
	}
	if(params_list)
		free_params(params_list);
	return 0;
}

/**
 *
 */
ds_dest_t *pack_dest(str iuri, int flags, int priority, str *attrs)
{
	ds_dest_t *dp = NULL;
	/* For DNS-Lookups */
	static char hn[256];
	char ub[512];
	struct hostent *he;
	struct sip_uri puri;
	str host;
	int port, proto;
	char c = 0;
	str uri;

	uri = iuri;
	/* check uri */
	if(parse_uri(uri.s, uri.len, &puri) != 0) {
		if(iuri.len>4 && strncmp(iuri.s, "sip:", 4)!=0 && iuri.len<500) {
			strncpy(ub, "sip:", 4);
			strncpy(ub+4, iuri.s, iuri.len);
			ub[iuri.len+4] = '\0';
			uri.s = ub;
			uri.len = iuri.len+4;
			if(parse_uri(uri.s, uri.len, &puri) != 0) {
				LM_ERR("bad uri [%.*s]\n", iuri.len, iuri.s);
				goto err;
			} else {
				LM_INFO("uri without sip scheme - fixing it: %.*s\n",
						iuri.len, iuri.s);
			}
		} else {
			LM_ERR("bad uri [%.*s]\n", iuri.len, iuri.s);
			goto err;
		}
	}

	if(puri.host.len > 254) {
		LM_ERR("hostname in uri is too long [%.*s]\n", uri.len, uri.s);
		goto err;
	}

	/* skip IPv6 references if IPv6 lookups are disabled */
	if(default_core_cfg.dns_try_ipv6 == 0 && puri.host.s[0] == '['
			&& puri.host.s[puri.host.len - 1] == ']') {
		LM_DBG("skipping IPv6 record %.*s\n", puri.host.len, puri.host.s);
		return NULL;
	}

	/* store uri */
	dp = (ds_dest_t *)shm_malloc(sizeof(ds_dest_t));
	if(dp == NULL) {
		LM_ERR("no more memory!\n");
		goto err;
	}
	memset(dp, 0, sizeof(ds_dest_t));

	dp->uri.s = (char *)shm_malloc((uri.len + 1) * sizeof(char));
	if(dp->uri.s == NULL) {
		LM_ERR("no more memory!\n");
		goto err;
	}
	strncpy(dp->uri.s, uri.s, uri.len);
	dp->uri.s[uri.len] = '\0';
	dp->uri.len = uri.len;

	dp->flags = flags;
	dp->priority = priority;

	if(ds_set_attrs(dp, attrs) < 0) {
		LM_ERR("cannot set attributes!\n");
		goto err;
	}

	/* check socket attribute */
	if(dp->attrs.socket.s && dp->attrs.socket.len > 0) {
		/* parse_phostport(...) expects 0-terminated string
		 * - after socket parameter is either ';' or '\0' */
		if(dp->attrs.socket.s[dp->attrs.socket.len] != '\0') {
			c = dp->attrs.socket.s[dp->attrs.socket.len];
			dp->attrs.socket.s[dp->attrs.socket.len] = '\0';
		}
		if(parse_phostport(
				   dp->attrs.socket.s, &host.s, &host.len, &port, &proto)
				!= 0) {
			LM_ERR("bad socket <%.*s>\n", dp->attrs.socket.len,
					dp->attrs.socket.s);
			if(c != 0) {
				dp->attrs.socket.s[dp->attrs.socket.len] = c;
			}
			goto err;
		}
		if(c != 0) {
			dp->attrs.socket.s[dp->attrs.socket.len] = c;
		}
		dp->sock = grep_sock_info(&host, (unsigned short)port, proto);
		if(dp->sock == 0) {
			LM_ERR("non-local socket <%.*s>\n", dp->attrs.socket.len,
					dp->attrs.socket.s);
			goto err;
		}
	} else if(ds_default_sockinfo) {
		dp->sock = ds_default_sockinfo;
	}

	/* The Hostname needs to be \0 terminated for resolvehost, so we
	 * make a copy here. */
	strncpy(hn, puri.host.s, puri.host.len);
	hn[puri.host.len] = '\0';

	/* Do a DNS-Lookup for the Host-Name: */
	he = resolvehost(hn);
	if(he == 0) {
		LM_ERR("could not resolve %.*s\n", puri.host.len, puri.host.s);
		goto err;
	}
	/* Free the hostname */
	hostent2ip_addr(&dp->ip_address, he, 0);

	/* Copy the port out of the URI */
	dp->port = puri.port_no;
	/* Copy the proto out of the URI */
	dp->proto = puri.proto;

	return dp;
err:
	if(dp != NULL) {
		if(dp->uri.s != NULL)
			shm_free(dp->uri.s);
		shm_free(dp);
	}

	return NULL;
}

/**
 *
 */
int add_dest2list(int id, str uri, int flags, int priority, str *attrs,
		int list_idx, int *setn)
{
	ds_dest_t *dp = NULL;
	ds_set_t *sp = NULL;
	ds_dest_t *dp0 = NULL;
	ds_dest_t *dp1 = NULL;

	dp = pack_dest(uri, flags, priority, attrs);
	if(!dp)
		goto err;

	sp = ds_avl_insert(&ds_lists[list_idx], id, setn);
	if(!sp) {
		LM_ERR("no more memory.\n");
		goto err;
	}
	sp->nr++;

	if(sp->dlist == NULL) {
		sp->dlist = dp;
	} else {
		dp1 = NULL;
		dp0 = sp->dlist;
		/* highest priority last -> reindex will copy backwards */
		while(dp0) {
			if(dp0->priority > dp->priority)
				break;
			dp1 = dp0;
			dp0 = dp0->next;
		}
		if(dp1 == NULL) {
			dp->next = sp->dlist;
			sp->dlist = dp;
		} else {
			dp->next = dp1->next;
			dp1->next = dp;
		}
	}

	LM_DBG("dest [%d/%d] <%.*s>\n", sp->id, sp->nr, dp->uri.len, dp->uri.s);

	return 0;
err:
	if(dp != NULL) {
		if(dp->uri.s != NULL)
			shm_free(dp->uri.s);
		shm_free(dp);
	}

	return -1;
}


/* for internal usage; arr must be arr[100] */
void shuffle_uint100array(unsigned int *arr)
{
	if(arr == NULL)
		return;
	int k;
	int j;
	unsigned int t;
	kam_srand(time(0));
	for(j = 0; j < 100; j++) {
		k = j + (kam_rand() % (100 - j));
		t = arr[j];
		arr[j] = arr[k];
		arr[k] = t;
	}
}


/**
 * Initialize the relative weight distribution for a destination set
 * - fill the array of 0..99 elements where to keep the index of the
 *   destination address to be used. The Nth call will use
 *   the address with the index at possition N%100
 */
int dp_init_relative_weights(ds_set_t *dset)
{
	int j;
	int k;
	int t;

	if(dset == NULL || dset->dlist == NULL)
		return -1;

	int rw_sum = 0;
	/* find the sum of relative weights*/
	for(j = 0; j < dset->nr; j++) {
		if(ds_skip_dst(dset->dlist[j].flags))
			continue;
		rw_sum += dset->dlist[j].attrs.rweight;
	}

	if(rw_sum == 0) {
		return 0;
	}

	/* fill the array based on the relative weight of each destination */
	t = 0;
	for(j = 0; j < dset->nr; j++) {
		if(ds_skip_dst(dset->dlist[j].flags))
			continue;

		int current_slice =
				dset->dlist[j].attrs.rweight * 100 / rw_sum; //truncate here;
		for(k = 0; k < current_slice; k++) {
			dset->rwlist[t] = (unsigned int)j;
			t++;
		}
	}
	/* if the array was not completely filled (i.e., the sum of rweights is
	 * less than 100 due to truncated), then use last address to fill the rest */
	unsigned int last_insert =
			t > 0 ? dset->rwlist[t - 1] : (unsigned int)(dset->nr - 1);
	for(j = t; j < 100; j++)
		dset->rwlist[j] = last_insert;

	/* shuffle the content of the array in order to mix the selection
	 * of the addresses (e.g., if first address has weight=20, avoid
	 * sending first 20 calls to it, but ensure that within a 100 calls,
	 * 20 go to first address */
	shuffle_uint100array(dset->rwlist);

	return 0;
}


/**
 * Initialize the weight distribution for a destination set
 * - fill the array of 0..99 elements where to keep the index of the
 *   destination address to be used. The Nth call will use
 *   the address with the index at possition N%100
 */
int dp_init_weights(ds_set_t *dset)
{
	int j;
	int k;
	int t;

	if(dset == NULL || dset->dlist == NULL)
		return -1;

	/* is weight set for dst list? (first address must have weight!=0) */
	if(dset->dlist[0].attrs.weight == 0)
		return 0;

	/* first fill the array based on the weight of each destination
	 * - the weight is the percentage (e.g., if weight=20, the afferent
	 *   address gets its index 20 times in the array)
	 * - if the sum of weights is more than 100, the addresses over the
	 *   limit are ignored */
	t = 0;
	for(j = 0; j < dset->nr; j++) {
		for(k = 0; k < dset->dlist[j].attrs.weight; k++) {
			if(t >= 100)
				goto randomize;
			dset->wlist[t] = (unsigned int)j;
			t++;
		}
	}
	/* if the array was not completely filled (i.e., the sum of weights is
	 * less than 100), then use last address to fill the rest */
	for(; t < 100; t++)
		dset->wlist[t] = (unsigned int)(dset->nr - 1);
randomize:
	/* shuffle the content of the array in order to mix the selection
	 * of the addresses (e.g., if first address has weight=20, avoid
	 * sending first 20 calls to it, but ensure that within a 100 calls,
	 * 20 go to first address */
	shuffle_uint100array(dset->wlist);

	return 0;
}

/*! \brief  compact destinations from sets for fast access */
int reindex_dests(ds_set_t *node)
{
	int i = 0;
	int j = 0;

	if(!node)
		return 0;

	for(; i < 2; ++i) {
		int rc = reindex_dests(node->next[i]);
		if(rc != 0)
			return rc;
	}

	ds_dest_t *dp = NULL, *dp0 = NULL;

	dp0 = (ds_dest_t *)shm_malloc(node->nr * sizeof(ds_dest_t));
	if(dp0 == NULL) {
		LM_ERR("no more memory!\n");
		goto err1;
	}
	memset(dp0, 0, node->nr * sizeof(ds_dest_t));

	/* copy from the old pointer to destination, and then free it */
	for(j = node->nr - 1; j >= 0 && node->dlist != NULL; j--) {
		memcpy(&dp0[j], node->dlist, sizeof(ds_dest_t));
		if(j == node->nr - 1)
			dp0[j].next = NULL;
		else
			dp0[j].next = &dp0[j + 1];


		dp = node->dlist;
		node->dlist = dp->next;

		shm_free(dp);
		dp = NULL;
	}
	node->dlist = dp0;
	dp_init_weights(node);
	dp_init_relative_weights(node);

	return 0;

err1:
	return -1;
}

/*! \brief load groups of destinations from file */
int ds_load_list(char *lfile)
{
	char line[1024], *p;
	FILE *f = NULL;
	int id, setn, flags, priority;
	str uri;
	str attrs;

	if((*crt_idx) != (*next_idx)) {
		LM_WARN("load command already generated, aborting reload...\n");
		return 0;
	}

	if(lfile == NULL || strlen(lfile) <= 0) {
		LM_ERR("bad list file\n");
		return -1;
	}

	f = fopen(lfile, "r");
	if(f == NULL) {
		LM_ERR("can't open list file [%s]\n", lfile);
		return -1;
	}

	id = setn = flags = priority = 0;

	*next_idx = (*crt_idx + 1) % 2;
	ds_avl_destroy(&ds_lists[*next_idx]);

	p = fgets(line, 1024, f);
	while(p) {
		/* eat all white spaces */
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;
		if(*p == '\0' || *p == '#')
			goto next_line;

		/* get set id */
		id = 0;
		while(*p >= '0' && *p <= '9') {
			id = id * 10 + (*p - '0');
			p++;
		}

		/* eat all white spaces */
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;
		if(*p == '\0' || *p == '#') {
			LM_ERR("bad line [%s]\n", line);
			goto error;
		}

		/* get uri */
		uri.s = p;
		while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'
				&& *p != '#')
			p++;
		uri.len = p - uri.s;

		/* eat all white spaces */
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;

		/* get flags */
		flags = 0;
		priority = 0;
		attrs.s = 0;
		attrs.len = 0;
		if(*p == '\0' || *p == '#')
			goto add_destination; /* no flags given */

		while(*p >= '0' && *p <= '9') {
			flags = flags * 10 + (*p - '0');
			p++;
		}

		/* eat all white spaces */
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;

		/* get priority */
		if(*p == '\0' || *p == '#')
			goto add_destination; /* no priority given */

		while(*p >= '0' && *p <= '9') {
			priority = priority * 10 + (*p - '0');
			p++;
		}

		/* eat all white spaces */
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;
		if(*p == '\0' || *p == '#')
			goto add_destination; /* no attrs given */

		/* get attributes */
		attrs.s = p;
		while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
			p++;
		attrs.len = p - attrs.s;

	add_destination:
		if(add_dest2list(id, uri, flags, priority, &attrs, *next_idx, &setn)
				!= 0)
			LM_WARN("unable to add destination %.*s to set %d -- skipping\n",
					uri.len, uri.s, id);
	next_line:
		p = fgets(line, 1024, f);
	}

	if(reindex_dests(ds_lists[*next_idx]) != 0) {
		LM_ERR("error on reindex\n");
		goto error;
	}

	LM_DBG("found [%d] dest sets\n", _ds_list_nr);

	fclose(f);
	f = NULL;
	/* Update list - should it be sync'ed? */
	_ds_list_nr = setn;
	*crt_idx = *next_idx;
	ds_ht_clear_slots(_dsht_load);
	ds_log_sets();
	return 0;

error:
	if(f != NULL)
		fclose(f);
	ds_avl_destroy(&ds_lists[*next_idx]);
	*next_idx = *crt_idx;
	return -1;
}

/**
 *
 */
int ds_connect_db(void)
{
	if(ds_db_url.s == NULL)
		return -1;

	if((ds_db_handle = ds_dbf.init(&ds_db_url)) == 0) {
		LM_ERR("cannot initialize db connection\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
void ds_disconnect_db(void)
{
	if(ds_db_handle) {
		ds_dbf.close(ds_db_handle);
		ds_db_handle = 0;
	}
}

/*! \brief Initialize and verify DB stuff*/
int init_ds_db(void)
{
	int ret;

	if(ds_table_name.s == 0) {
		LM_ERR("invalid database name\n");
		return -1;
	}

	/* Find a database module */
	if(db_bind_mod(&ds_db_url, &ds_dbf) < 0) {
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}

	if(ds_connect_db() != 0) {
		LM_ERR("unable to connect to the database\n");
		return -1;
	}

	_ds_table_version = db_table_version(&ds_dbf, ds_db_handle, &ds_table_name);
	if(_ds_table_version < 0) {
		LM_ERR("failed to query table version\n");
		return -1;
	} else if(_ds_table_version != DS_TABLE_VERSION
			  && _ds_table_version != DS_TABLE_VERSION2
			  && _ds_table_version != DS_TABLE_VERSION3
			  && _ds_table_version != DS_TABLE_VERSION4) {
		LM_ERR("invalid table version (found %d , required %d, %d, %d or %d)\n"
			   "(use kamdbctl reinit)\n",
				_ds_table_version, DS_TABLE_VERSION, DS_TABLE_VERSION2,
				DS_TABLE_VERSION3, DS_TABLE_VERSION4);
		return -1;
	}

	ret = ds_load_db();
	if(ret == -2) {
		LM_WARN("failure while loading one or more dispatcher entries\n");
		ret = 0;
	}

	ds_disconnect_db();

	return ret;
}

/*! \brief reload groups of destinations from DB*/
int ds_reload_db(void)
{
	int ret;

	if(ds_connect_db() != 0) {
		LM_ERR("unable to connect to the database\n");
		return -1;
	}
	ret = ds_load_db();
	if(ret == -2) {
		LM_WARN("failure while loading one or more dispatcher entries\n");
	}
	ds_disconnect_db();

	return ret;
}

/*! \brief load groups of destinations from DB*/
int ds_load_db(void)
{
	int i, id, nr_rows, setn;
	int flags;
	int priority;
	int nrcols;
	int dest_errs = 0;
	str uri;
	str attrs = {0, 0};
	db1_res_t *res;
	db_val_t *values;
	db_row_t *rows;

	db_key_t query_cols[5] = {&ds_set_id_col, &ds_dest_uri_col,
			&ds_dest_flags_col, &ds_dest_priority_col, &ds_dest_attrs_col};

	nrcols = 2;
	if(_ds_table_version == DS_TABLE_VERSION2)
		nrcols = 3;
	else if(_ds_table_version == DS_TABLE_VERSION3)
		nrcols = 4;
	else if(_ds_table_version == DS_TABLE_VERSION4)
		nrcols = 5;

	if((*crt_idx) != (*next_idx)) {
		LM_WARN("load command already generated, aborting reload...\n");
		return 0;
	}

	if(ds_db_handle == NULL) {
		LM_ERR("invalid DB handler\n");
		return -1;
	}

	if(ds_dbf.use_table(ds_db_handle, &ds_table_name) < 0) {
		LM_ERR("error in use_table\n");
		return -1;
	}

	/*select the whole table and all the columns*/
	if(ds_dbf.query(ds_db_handle, 0, 0, 0, query_cols, 0, nrcols, 0, &res)
			< 0) {
		LM_ERR("error while querying database\n");
		return -1;
	}

	nr_rows = RES_ROW_N(res);
	rows = RES_ROWS(res);
	if(nr_rows == 0)
		LM_WARN("no dispatching data in the db -- empty destination set\n");

	setn = 0;
	*next_idx = (*crt_idx + 1) % 2;
	ds_avl_destroy(&ds_lists[*next_idx]);

	for(i = 0; i < nr_rows; i++) {
		values = ROW_VALUES(rows + i);

		id = VAL_INT(values);
		uri.s = VAL_STR(values + 1).s;
		uri.len = strlen(uri.s);
		flags = 0;
		if(nrcols >= 3)
			flags = VAL_INT(values + 2);
		priority = 0;
		if(nrcols >= 4)
			priority = VAL_INT(values + 3);

		attrs.s = 0;
		attrs.len = 0;
		if(nrcols >= 5) {
			attrs.s = VAL_STR(values + 4).s;
			attrs.len = strlen(attrs.s);
		}
		if(add_dest2list(id, uri, flags, priority, &attrs, *next_idx, &setn)
				!= 0) {
			dest_errs++;
			LM_WARN("unable to add destination %.*s to set %d -- skipping\n",
					uri.len, uri.s, id);
		}
	}
	if(reindex_dests(ds_lists[*next_idx]) != 0) {
		LM_ERR("error on reindex\n");
		goto err2;
	}

	LM_DBG("found [%d] dest sets\n", _ds_list_nr);

	ds_dbf.free_result(ds_db_handle, res);

	/* update data - should it be sync'ed? */
	_ds_list_nr = setn;
	*crt_idx = *next_idx;
	ds_ht_clear_slots(_dsht_load);

	ds_log_sets();

	if(dest_errs > 0)
		return -2;
	return 0;

err2:
	ds_avl_destroy(&ds_lists[*next_idx]);
	ds_dbf.free_result(ds_db_handle, res);
	*next_idx = *crt_idx;

	return -1;
}

/*! \brief called from dispatcher.c: free all*/
int ds_destroy_list(void)
{
	if(ds_lists) {
		ds_avl_destroy(&ds_lists[0]);
		ds_avl_destroy(&ds_lists[1]);
		shm_free(ds_lists);
	}

	if(crt_idx)
		shm_free(crt_idx);

	return 0;
}

/**
 *
 */
unsigned int ds_get_hash(str *x, str *y)
{
	char *p;
	register unsigned v;
	register unsigned h;

	if(!x && !y)
		return 0;
	h = 0;
	if(x) {
		p = x->s;
		if(x->len >= 4) {
			for(; p <= (x->s + x->len - 4); p += 4) {
				v = (*p << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
				h += v ^ (v >> 3);
			}
		}
		v = 0;
		for(; p < (x->s + x->len); p++) {
			v <<= 8;
			v += *p;
		}
		h += v ^ (v >> 3);
	}
	if(y) {
		p = y->s;
		if(y->len >= 4) {
			for(; p <= (y->s + y->len - 4); p += 4) {
				v = (*p << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
				h += v ^ (v >> 3);
			}
		}

		v = 0;
		for(; p < (y->s + y->len); p++) {
			v <<= 8;
			v += *p;
		}
		h += v ^ (v >> 3);
	}
	h = ((h) + (h >> 11)) + ((h >> 13) + (h >> 23));

	return (h) ? h : 1;
}


/*! \brief
 * gets the part of the uri we will use as a key for hashing
 * \param  key1       - will be filled with first part of the key
 *                       (uri user or "" if no user)
 * \param  key2       - will be filled with the second part of the key
 *                       (uri host:port)
 * \param  uri        - str with the whole uri
 * \param  parsed_uri - struct sip_uri pointer with the parsed uri
 *                       (it must point inside uri). It can be null
 *                       (in this case the uri will be parsed internally).
 * \param  flags  -    if & DS_HASH_USER_ONLY, only the user part of the uri
 *                      will be used
 * \return: -1 on error, 0 on success
 */
static inline int get_uri_hash_keys(
		str *key1, str *key2, str *uri, struct sip_uri *parsed_uri, int flags)
{
	struct sip_uri tmp_p_uri; /* used only if parsed_uri==0 */

	if(parsed_uri == 0) {
		if(parse_uri(uri->s, uri->len, &tmp_p_uri) < 0) {
			LM_ERR("invalid uri %.*s\n", uri->len, uri->len ? uri->s : "");
			goto error;
		}
		parsed_uri = &tmp_p_uri;
	}
	/* uri sanity checks */
	if(parsed_uri->host.s == 0) {
		LM_ERR("invalid uri, no host present: %.*s\n", uri->len,
				uri->len ? uri->s : "");
		goto error;
	}

	/* we want: user@host:port if port !=5060
	 *          user@host if port==5060
	 *          user if the user flag is set*/
	*key1 = parsed_uri->user;
	key2->s = 0;
	key2->len = 0;
	if(!(flags & DS_HASH_USER_ONLY)) { /* key2=host */
		*key2 = parsed_uri->host;
		/* add port if needed */
		if(parsed_uri->port.s != 0) { /* uri has a port */
			/* skip port if == 5060 or sips and == 5061 */
			if(parsed_uri->port_no != ((parsed_uri->type == SIPS_URI_T)
													  ? SIPS_PORT
													  : SIP_PORT))
				key2->len += parsed_uri->port.len + 1 /* ':' */;
		}
	}
	if(key1->s == 0) {
		LM_WARN("empty username in: %.*s\n", uri->len, uri->len ? uri->s : "");
	}
	return 0;
error:
	return -1;
}


/**
 *
 */
int ds_hash_fromuri(struct sip_msg *msg, unsigned int *hash)
{
	str from;
	str key1;
	str key2;

	if(msg == NULL || hash == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(parse_from_header(msg) < 0) {
		LM_ERR("cannot parse From hdr\n");
		return -1;
	}

	if(msg->from == NULL || get_from(msg) == NULL) {
		LM_ERR("cannot get From uri\n");
		return -1;
	}

	from = get_from(msg)->uri;
	trim(&from);
	if(get_uri_hash_keys(&key1, &key2, &from, 0, ds_flags) < 0)
		return -1;
	*hash = ds_get_hash(&key1, &key2);

	return 0;
}


/**
 *
 */
int ds_hash_touri(struct sip_msg *msg, unsigned int *hash)
{
	str to;
	str key1;
	str key2;

	if(msg == NULL || hash == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}
	if((msg->to == 0)
			&& ((parse_headers(msg, HDR_TO_F, 0) == -1) || (msg->to == 0))) {
		LM_ERR("cannot parse To hdr\n");
		return -1;
	}


	to = get_to(msg)->uri;
	trim(&to);

	if(get_uri_hash_keys(&key1, &key2, &to, 0, ds_flags) < 0)
		return -1;
	*hash = ds_get_hash(&key1, &key2);

	return 0;
}


/**
 *
 */
int ds_hash_callid(struct sip_msg *msg, unsigned int *hash)
{
	str cid;
	if(msg == NULL || hash == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(msg->callid == NULL && ((parse_headers(msg, HDR_CALLID_F, 0) == -1)
									  || (msg->callid == NULL))) {
		LM_ERR("cannot parse Call-Id\n");
		return -1;
	}

	cid.s = msg->callid->body.s;
	cid.len = msg->callid->body.len;
	trim(&cid);

	*hash = ds_get_hash(&cid, NULL);

	return 0;
}


/**
 *
 */
int ds_hash_ruri(struct sip_msg *msg, unsigned int *hash)
{
	str *uri;
	str key1;
	str key2;


	if(msg == NULL || hash == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}
	if(parse_sip_msg_uri(msg) < 0) {
		LM_ERR("bad request uri\n");
		return -1;
	}

	uri = GET_RURI(msg);
	if(get_uri_hash_keys(&key1, &key2, uri, &msg->parsed_uri, ds_flags) < 0)
		return -1;

	*hash = ds_get_hash(&key1, &key2);
	return 0;
}

/**
 *
 */
int ds_hash_authusername(struct sip_msg *msg, unsigned int *hash)
{
	/* Header, which contains the authorization */
	struct hdr_field *h = 0;
	/* The Username */
	str username = {0, 0};
	/* The Credentials from this request */
	auth_body_t *cred;

	if(msg == NULL || hash == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}
	*hash = 0;
	if(parse_headers(msg, HDR_PROXYAUTH_F, 0) == -1) {
		LM_ERR("error parsing headers!\n");
		return -1;
	}
	if(msg->proxy_auth && !msg->proxy_auth->parsed) {
		if(parse_credentials(msg->proxy_auth)!=0) {
			LM_DBG("no parsing for proxy-auth header\n");
		}
	}
	if(msg->proxy_auth && msg->proxy_auth->parsed) {
		h = msg->proxy_auth;
	}
	if(!h) {
		if(parse_headers(msg, HDR_AUTHORIZATION_F, 0) == -1) {
			LM_ERR("error parsing headers!\n");
			return -1;
		}
		if(msg->authorization && !msg->authorization->parsed) {
			if(parse_credentials(msg->authorization)!=0) {
				LM_DBG("no parsing for auth header\n");
			}
		}
		if(msg->authorization && msg->authorization->parsed) {
			h = msg->authorization;
		}
	}
	if(!h) {
		LM_DBG("No Authorization-Header!\n");
		return 1;
	}

	cred = (auth_body_t *)(h->parsed);
	if(!cred || !cred->digest.username.user.len) {
		LM_ERR("No Authorization-Username or Credentials!\n");
		return 1;
	}

	username.s = cred->digest.username.user.s;
	username.len = cred->digest.username.user.len;

	trim(&username);

	*hash = ds_get_hash(&username, NULL);

	return 0;
}


/**
 *
 */
int ds_hash_pvar(struct sip_msg *msg, unsigned int *hash)
{
	/* The String to create the hash */
	str hash_str = {0, 0};

	if(msg == NULL || hash == NULL || hash_param_model == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}
	if(pv_printf_s(msg, hash_param_model, &hash_str) < 0) {
		LM_ERR("error - cannot print the format\n");
		return -1;
	}

	/* Remove empty spaces */
	trim(&hash_str);
	if(hash_str.len <= 0) {
		LM_ERR("String is empty!\n");
		return -1;
	}

	*hash = ds_get_hash(&hash_str, NULL);
	LM_DBG("Hashing of '%.*s' resulted in %u !\n", hash_str.len, hash_str.s,
			*hash);

	return 0;
}

/**
 *
 */
static inline int ds_get_index(int group, int ds_list_idx, ds_set_t **index)
{
	ds_set_t *si = NULL;

	if(index == NULL || group < 0 || ds_lists[ds_list_idx] == NULL)
		return -1;

	/* get the index of the set */
	si = ds_avl_find(ds_lists[ds_list_idx], group);

	if(si == NULL)
		return -1;

	*index = si;
	return 0;
}

/*
 * Check if a destination set exists
 */
int ds_list_exist(int set)
{
	ds_set_t *si = NULL;
	LM_DBG("-- Looking for set %d\n", set);

	/* get the index of the set */
	si = ds_avl_find(_ds_list, set);

	if(si == NULL) {
		LM_INFO("destination set [%d] not found\n", set);
		return -1; /* False */
	}
	LM_INFO("destination set [%d] found\n", set);
	return 1; /* True */
}

/**
 *
 */
int ds_get_leastloaded(ds_set_t *dset)
{
	int j;
	int k;
	int t;

	k = -1;
	t = 0x7fffffff; /* high load */
	for(j = 0; j < dset->nr; j++) {
		if(!ds_skip_dst(dset->dlist[j].flags)
				&& (dset->dlist[j].attrs.maxload == 0
						   || dset->dlist[j].dload
									  < dset->dlist[j].attrs.maxload)) {
			if(dset->dlist[j].dload < t) {
				k = j;
				t = dset->dlist[k].dload;
			}
		}
	}
	return k;
}

/**
 *
 */
int ds_load_add(struct sip_msg *msg, ds_set_t *dset, int setid, int dst)
{
	if(dset->dlist[dst].attrs.duid.len == 0) {
		LM_ERR("dst unique id not set for %d (%.*s)\n", setid,
				msg->callid->body.len, msg->callid->body.s);
		return -1;
	}

	if(ds_add_cell(_dsht_load, &msg->callid->body, &dset->dlist[dst].attrs.duid,
			   setid)
			< 0) {
		LM_ERR("cannot add load to %d (%.*s)\n", setid, msg->callid->body.len,
				msg->callid->body.s);
		return -1;
	}
	dset->dlist[dst].dload++;
	return 0;
}

/**
 *
 */
int ds_load_replace(struct sip_msg *msg, str *duid)
{
	ds_cell_t *it;
	int set;
	int olddst;
	int newdst;
	ds_set_t *idx = NULL;
	int i;

	if(duid->len <= 0) {
		LM_ERR("invalid dst unique id not set for (%.*s)\n",
				msg->callid->body.len, msg->callid->body.s);
		return -1;
	}

	if((it = ds_get_cell(_dsht_load, &msg->callid->body)) == NULL) {
		LM_ERR("cannot find load for (%.*s)\n", msg->callid->body.len,
				msg->callid->body.s);
		return -1;
	}
	set = it->dset;
	/* get the index of the set */
	if(ds_get_index(set, *crt_idx, &idx) != 0) {
		ds_unlock_cell(_dsht_load, &msg->callid->body);
		LM_ERR("destination set [%d] not found\n", set);
		return -1;
	}
	olddst = -1;
	newdst = -1;
	for(i = 0; i < idx->nr; i++) {
		if(idx->dlist[i].attrs.duid.len == it->duid.len
				&& strncasecmp(
						   idx->dlist[i].attrs.duid.s, it->duid.s, it->duid.len)
						   == 0) {
			olddst = i;
			if(newdst != -1)
				break;
		}
		if(idx->dlist[i].attrs.duid.len == duid->len
				&& strncasecmp(idx->dlist[i].attrs.duid.s, duid->s, duid->len)
						   == 0) {
			newdst = i;
			if(olddst != -1)
				break;
		}
	}
	if(olddst == -1) {
		ds_unlock_cell(_dsht_load, &msg->callid->body);
		LM_ERR("old destination address not found for [%d, %.*s]\n", set,
				it->duid.len, it->duid.s);
		return -1;
	}
	if(newdst == -1) {
		ds_unlock_cell(_dsht_load, &msg->callid->body);
		LM_ERR("new destination address not found for [%d, %.*s]\n", set,
				duid->len, duid->s);
		return -1;
	}

	ds_unlock_cell(_dsht_load, &msg->callid->body);
	ds_del_cell(_dsht_load, &msg->callid->body);
	if(idx->dlist[olddst].dload > 0)
		idx->dlist[olddst].dload--;

	if(ds_load_add(msg, idx, set, newdst) < 0) {
		LM_ERR("unable to replace destination load [%.*s / %.*s]\n", duid->len,
				duid->s, msg->callid->body.len, msg->callid->body.s);
		return -1;
	}
	return 0;
}

/**
 *
 */
int ds_load_remove(struct sip_msg *msg)
{
	ds_cell_t *it;
	int set;
	int olddst;
	ds_set_t *idx = NULL;
	int i;

	if((it = ds_get_cell(_dsht_load, &msg->callid->body)) == NULL) {
		LM_ERR("cannot find load for (%.*s)\n", msg->callid->body.len,
				msg->callid->body.s);
		return -1;
	}
	set = it->dset;
	/* get the index of the set */
	if(ds_get_index(set, *crt_idx, &idx) != 0) {
		ds_unlock_cell(_dsht_load, &msg->callid->body);
		LM_ERR("destination set [%d] not found\n", set);
		return -1;
	}
	olddst = -1;
	for(i = 0; i < idx->nr; i++) {
		if(idx->dlist[i].attrs.duid.len == it->duid.len
				&& strncasecmp(
						   idx->dlist[i].attrs.duid.s, it->duid.s, it->duid.len)
						   == 0) {
			olddst = i;
			break;
		}
	}
	if(olddst == -1) {
		ds_unlock_cell(_dsht_load, &msg->callid->body);
		LM_ERR("old destination address not found for [%d, %.*s]\n", set,
				it->duid.len, it->duid.s);
		return -1;
	}

	ds_unlock_cell(_dsht_load, &msg->callid->body);
	ds_del_cell(_dsht_load, &msg->callid->body);
	if(idx->dlist[olddst].dload > 0)
		idx->dlist[olddst].dload--;

	return 0;
}


/**
 *
 */
int ds_load_remove_byid(int set, str *duid)
{
	int olddst;
	ds_set_t *idx = NULL;
	int i;

	/* get the index of the set */
	if(ds_get_index(set, *crt_idx, &idx) != 0) {
		LM_ERR("destination set [%d] not found\n", set);
		return -1;
	}
	olddst = -1;
	for(i = 0; i < idx->nr; i++) {
		if(idx->dlist[i].attrs.duid.len == duid->len
				&& strncasecmp(idx->dlist[i].attrs.duid.s, duid->s, duid->len)
						   == 0) {
			olddst = i;
			break;
		}
	}
	if(olddst == -1) {
		LM_ERR("old destination address not found for [%d, %.*s]\n", set,
				duid->len, duid->s);
		return -1;
	}

	if(idx->dlist[olddst].dload > 0)
		idx->dlist[olddst].dload--;

	return 0;
}

/**
 *
 */
int ds_load_state(struct sip_msg *msg, int state)
{
	ds_cell_t *it;

	if((it = ds_get_cell(_dsht_load, &msg->callid->body)) == NULL) {
		LM_DBG("cannot find load for (%.*s)\n", msg->callid->body.len,
				msg->callid->body.s);
		return -1;
	}

	it->state = state;
	ds_unlock_cell(_dsht_load, &msg->callid->body);

	return 0;
}


/**
 *
 */
int ds_load_update(struct sip_msg *msg)
{
	if(parse_headers(msg, HDR_CSEQ_F | HDR_CALLID_F, 0) != 0
			|| msg->cseq == NULL || msg->callid == NULL) {
		LM_ERR("cannot parse cseq and callid headers\n");
		return -1;
	}
	if(msg->first_line.type == SIP_REQUEST) {
		if(msg->first_line.u.request.method_value == METHOD_BYE
				|| msg->first_line.u.request.method_value == METHOD_CANCEL) {
			/* off-load call */
			ds_load_remove(msg);
		}
		return 0;
	}

	if(get_cseq(msg)->method_id == METHOD_INVITE) {
		/* if status is 2xx then set state to confirmed */
		if(REPLY_CLASS(msg) == 2)
			ds_load_state(msg, DS_LOAD_CONFIRMED);
	}
	return 0;
}

/**
 *
 */
int ds_load_unset(struct sip_msg *msg)
{
	struct search_state st;
	struct usr_avp *prev_avp;
	int_str avp_value;

	if(dstid_avp_name.n == 0)
		return 0;

	/* for INVITE requests should be called after dst list is built */
	if(msg->first_line.type == SIP_REQUEST
			&& msg->first_line.u.request.method_value == METHOD_INVITE) {
		prev_avp = search_first_avp(
				dstid_avp_type, dstid_avp_name, &avp_value, &st);
		if(prev_avp == NULL)
			return 0;
	}
	return ds_load_remove(msg);
}

/**
 *
 */
static inline int ds_update_dst(
		struct sip_msg *msg, str *uri, struct socket_info *sock, int mode)
{
	struct action act;
	struct run_act_ctx ra_ctx;
	switch(mode) {
		case 1:
			memset(&act, '\0', sizeof(act));
			act.type = SET_HOSTALL_T;
			act.val[0].type = STRING_ST;
			if(uri->len > 4 && strncasecmp(uri->s, "sip:", 4) == 0) {
				act.val[0].u.string = uri->s + 4;
			} else if(uri->len > 5 && strncasecmp(uri->s, "sips:", 5) == 0) {
				act.val[0].u.string = uri->s + 5;
			} else {
				act.val[0].u.string = uri->s;
			}
			init_run_actions_ctx(&ra_ctx);
			if(do_action(&ra_ctx, &act, msg) < 0) {
				LM_ERR("error while setting r-uri domain with: %.*s\n",
						uri->len, uri->s);
				return -1;
			}
			break;

		case 2:
			/* no update to d-uri/r-uri */
			return 0;

		default:
			if(set_dst_uri(msg, uri) < 0) {
				LM_ERR("error while setting dst uri with: %.*s\n",
						uri->len, uri->s);
				return -1;
			}
			/* dst_uri changes, so it makes sense to re-use the current uri for
			 * forking */
			ruri_mark_new(); /* re-use uri for serial forking */
			break;
	}
	if(sock)
		msg->force_send_socket = sock;
	return 0;
}

/**
 *
 */
int ds_add_branches(sip_msg_t *msg, ds_set_t *idx, unsigned int hash, int mode)
{
	unsigned int i = 0;
	str ruri = STR_NULL;
	sip_uri_t *puri = NULL;
	char buri[MAX_URI_SIZE];

	if(hash+1>=idx->nr) {
		/* nothing to add */
		return 0;
	}

	if(mode==1) {
		/* ruri updates */
		LM_DBG("adding branches with ruri\n");
		if(parse_sip_msg_uri(msg)<0) {
			LM_ERR("failed to parse sip msg uri\n");
			return -1;
		}
		puri = &msg->parsed_uri;
	} else {
		/* duri updates */
		LM_DBG("adding branches with duri\n");
	}
	for(i=hash+1; i<idx->nr; i++) {
		if(mode==1) {
			/* ruri updates */
			if(puri->user.len<=0) {
				/* no username to preserve */
				if(append_branch(msg, &idx->dlist[i].uri, NULL, NULL,
						Q_UNSPECIFIED, 0, idx->dlist[i].sock, NULL, 0,
						NULL, NULL)<0) {
					LM_ERR("failed to add branch with ruri\n");
					return -1;
				}
			} else {
				/* new uri from ruri username and dispatcher uri */
				if(idx->dlist[i].uri.len<6) {
					LM_WARN("invalid dispatcher uri - skipping (%u)\n", i);
					continue;
				}
				if(strncmp(idx->dlist[i].uri.s, "sips:", 5)==0) {
					ruri.len = snprintf(buri, MAX_URI_SIZE, "sips:%.*s@%.*s",
							puri->user.len, puri->user.s,
							idx->dlist[i].uri.len-5, idx->dlist[i].uri.s+5);
				} else {
					if(strncmp(idx->dlist[i].uri.s, "sip:", 4)==0) {
						ruri.len = snprintf(buri, MAX_URI_SIZE, "sip:%.*s@%.*s",
								puri->user.len, puri->user.s,
								idx->dlist[i].uri.len-4, idx->dlist[i].uri.s+4);
					} else {
						LM_WARN("unsupported protocol schema - ignoring\n");
						continue;
					}
				}
				ruri.s = buri;
				if(append_branch(msg, &ruri, NULL, NULL,
						Q_UNSPECIFIED, 0, idx->dlist[i].sock, NULL, 0,
						NULL, NULL)<0) {
					LM_ERR("failed to add branch with user ruri\n");
					return -1;
				}
			}
		} else {
			/* duri updates */
			if(append_branch(msg, GET_RURI(msg), &idx->dlist[i].uri, NULL,
					Q_UNSPECIFIED, 0, idx->dlist[i].sock, NULL, 0,
					NULL, NULL)<0) {
				LM_ERR("failed to add branch with duri\n");
				return -1;
			}
		}

	}
	return 0;
}

/**
 *
 */
int ds_select_dst(struct sip_msg *msg, int set, int alg, int mode)
{
	return ds_select_dst_limit(msg, set, alg, 0, mode);
}

/**
 * Set destination address from group 'set' selected with alogorithm 'alg'
 * - the rest of addresses in group are added as next destination in avps,
 *   up to the 'limit'
 * - mode specify to set address in R-URI or outboud proxy
 *
 */
int ds_select_dst_limit(
		sip_msg_t *msg, int set, int alg, unsigned int limit, int mode)
{
	int i, cnt;
	unsigned int hash, lhash;
	int_str avp_val;
	ds_set_t *idx = NULL;
	char buf[2 + 16 + 1];

	if(msg == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(_ds_list == NULL || _ds_list_nr <= 0) {
		LM_ERR("no destination sets\n");
		return -1;
	}

	if(limit == 0) {
		LM_DBG("Limit set to 0 - forcing to unlimited\n");
		limit = 0xffffffff;
	}
	--limit; /* reserving 1 slot for selected dst */

	if((mode == 0) && (ds_force_dst == 0)
			&& (msg->dst_uri.s != NULL || msg->dst_uri.len > 0)) {
		LM_ERR("destination already set [%.*s]\n", msg->dst_uri.len,
				msg->dst_uri.s);
		return -1;
	}


	/* get the index of the set */
	if(ds_get_index(set, *crt_idx, &idx) != 0) {
		LM_ERR("destination set [%d] not found\n", set);
		return -1;
	}

	LM_DBG("set [%d]\n", set);

	hash = 0;
	switch(alg) {
		case DS_ALG_HASHCALLID: /* 0 - hash call-id */
			if(ds_hash_callid(msg, &hash) != 0) {
				LM_ERR("can't get callid hash\n");
				return -1;
			}
			break;
		case DS_ALG_HASHFROMURI: /* 1 - hash from-uri */
			if(ds_hash_fromuri(msg, &hash) != 0) {
				LM_ERR("can't get From uri hash\n");
				return -1;
			}
			break;
		case DS_ALG_HASHTOURI: /* 2 - hash to-uri */
			if(ds_hash_touri(msg, &hash) != 0) {
				LM_ERR("can't get To uri hash\n");
				return -1;
			}
			break;
		case DS_ALG_HASHRURI: /* 3 - hash r-uri */
			if(ds_hash_ruri(msg, &hash) != 0) {
				LM_ERR("can't get ruri hash\n");
				return -1;
			}
			break;
		case DS_ALG_ROUNDROBIN: /* 4 - round robin */
			hash = idx->last;
			idx->last = (idx->last + 1) % idx->nr;
			break;
		case DS_ALG_HASHAUTHUSER: /* 5 - hash auth username */
			i = ds_hash_authusername(msg, &hash);
			switch(i) {
				case 0:
					/* Authorization-Header found: Nothing to be done here */
					break;
				case 1:
					/* No Authorization found: Use round robin */
					hash = idx->last;
					idx->last = (idx->last + 1) % idx->nr;
					break;
				default:
					LM_ERR("can't get authorization hash\n");
					return -1;
			}
			break;
		case DS_ALG_RANDOM: /* 6 - random selection */
			hash = kam_rand() % idx->nr;
			break;
		case DS_ALG_HASHPV: /* 7 - hash on PV value */
			if(ds_hash_pvar(msg, &hash) != 0) {
				LM_ERR("can't get PV hash\n");
				return -1;
			}
			break;
		case DS_ALG_SERIAL: /* 8 - use always first entry */
			hash = 0;
			break;
		case DS_ALG_WEIGHT: /* 9 - weight based distribution */
			hash = idx->wlist[idx->wlast];
			idx->wlast = (idx->wlast + 1) % 100;
			break;
		case DS_ALG_CALLLOAD: /* 10 - call load based distribution */
			/* only INVITE can start a call */
			if(msg->first_line.u.request.method_value != METHOD_INVITE) {
				/* use first entry */
				hash = 0;
				alg = 0;
				break;
			}
			if(dstid_avp_name.n == 0) {
				LM_ERR("no dst ID avp for load distribution"
					   " - using first entry...\n");
				hash = 0;
				alg = 0;
			} else {
				i = ds_get_leastloaded(idx);
				if(i < 0) {
					/* no address selected */
					return -1;
				}
				hash = i;
				if(ds_load_add(msg, idx, set, hash) < 0) {
					LM_ERR("unable to update destination load"
						   " - classic dispatching\n");
					alg = 0;
				}
			}
			break;
		case DS_ALG_RELWEIGHT: /* 11 - relative weight based distribution */
			hash = idx->rwlist[idx->rwlast];
			idx->rwlast = (idx->rwlast + 1) % 100;
			break;
		case DS_ALG_PARALLEL: /* 12 - parallel dispatching */
			hash = 0;
			break;
		default:
			LM_WARN("algo %d not implemented - using first entry...\n", alg);
			hash = 0;
	}

	LM_DBG("alg hash [%u]\n", hash);
	cnt = 0;

	if(ds_use_default != 0 && idx->nr != 1)
		hash = hash % (idx->nr - 1);
	else
		hash = hash % idx->nr;
	i = hash;

	/* if selected address is inactive, find next active */
	while(ds_skip_dst(idx->dlist[i].flags)) {
		if(ds_use_default != 0 && idx->nr != 1)
			i = (i + 1) % (idx->nr - 1);
		else
			i = (i + 1) % idx->nr;
		if(i == hash) {
			/* back to start -- looks like no active dst */
			if(ds_use_default != 0) {
				i = idx->nr - 1;
				if(ds_skip_dst(idx->dlist[i].flags))
					return -1;
				break;
			} else {
				return -1;
			}
		}
	}

	hash = i;

	if(ds_update_dst(msg, &idx->dlist[hash].uri, idx->dlist[hash].sock, mode)
			!= 0) {
		LM_ERR("cannot set next hop address with: %.*s\n",
				idx->dlist[hash].uri.len, idx->dlist[hash].uri.s);
		return -1;
	}
	/* if alg is round-robin then update the shortcut to next to be used */
	if(alg == DS_ALG_ROUNDROBIN)
		idx->last = (hash + 1) % idx->nr;

	LM_DBG("selected [%d-%d/%d] <%.*s>\n", alg, set, hash,
			idx->dlist[hash].uri.len, idx->dlist[hash].uri.s);

	if(alg == DS_ALG_PARALLEL) {
		if(ds_add_branches(msg, idx, hash, mode)<0) {
			LM_ERR("failed to add additional branches\n");
			/* one destination was already set - return success anyhow */
			return 2;
		}
		return 1;
	}

	if(!(ds_flags & DS_FAILOVER_ON))
		return 1;

	if(dst_avp_name.n != 0) {
		/* add default dst to last position in AVP list */
		if(ds_use_default != 0 && hash != idx->nr - 1 && cnt < limit) {
			avp_val.s = idx->dlist[idx->nr - 1].uri;
			if(add_avp(AVP_VAL_STR | dst_avp_type, dst_avp_name, avp_val) != 0)
				return -1;

			if(attrs_avp_name.n != 0
					&& idx->dlist[idx->nr - 1].attrs.body.len > 0) {
				avp_val.s = idx->dlist[idx->nr - 1].attrs.body;
				if(add_avp(AVP_VAL_STR | attrs_avp_type, attrs_avp_name,
						   avp_val)
						!= 0)
					return -1;
			}

			/* only add sock_avp if dst_avp is set */
			if(sock_avp_name.n != 0 && idx->dlist[idx->nr - 1].sock) {
				avp_val.s.len =
						1 + sprintf(buf, "%p", idx->dlist[idx->nr - 1].sock);
				avp_val.s.s = buf;
				if(add_avp(AVP_VAL_STR | sock_avp_type, sock_avp_name, avp_val)
						!= 0)
					return -1;
			}

			if(alg == DS_ALG_CALLLOAD) {
				if(idx->dlist[idx->nr - 1].attrs.duid.len <= 0) {
					LM_ERR("no uid for destination: %d %.*s\n", set,
							idx->dlist[idx->nr - 1].uri.len,
							idx->dlist[idx->nr - 1].uri.s);
					return -1;
				}
				avp_val.s = idx->dlist[idx->nr - 1].attrs.duid;
				if(add_avp(AVP_VAL_STR | dstid_avp_type, dstid_avp_name,
						   avp_val)
						!= 0)
					return -1;
			}
			cnt++;
		}

		/* add to avp in reverse order */
		for(i = hash - 1; i >= 0 && cnt < limit; i--) {
			if(ds_skip_dst(idx->dlist[i].flags)
					|| (ds_use_default != 0 && i == (idx->nr - 1)))
				continue;
			/* max load exceeded per destination */
			if(alg == DS_ALG_CALLLOAD
					&& idx->dlist[i].dload >= idx->dlist[i].attrs.maxload)
				continue;
			LM_DBG("using entry [%d/%d]\n", set, i);
			avp_val.s = idx->dlist[i].uri;
			if(add_avp(AVP_VAL_STR | dst_avp_type, dst_avp_name, avp_val) != 0)
				return -1;

			if(attrs_avp_name.n != 0 && idx->dlist[i].attrs.body.len > 0) {
				avp_val.s = idx->dlist[i].attrs.body;
				if(add_avp(AVP_VAL_STR | attrs_avp_type, attrs_avp_name,
						   avp_val)
						!= 0)
					return -1;
			}

			if(sock_avp_name.n != 0 && idx->dlist[i].sock) {
				avp_val.s.len = 1 + sprintf(buf, "%p", idx->dlist[i].sock);
				avp_val.s.s = buf;
				if(add_avp(AVP_VAL_STR | sock_avp_type, sock_avp_name, avp_val)
						!= 0)
					return -1;
			}

			if(alg == DS_ALG_CALLLOAD) {
				if(idx->dlist[i].attrs.duid.len <= 0) {
					LM_ERR("no uid for destination: %d %.*s\n", set,
							idx->dlist[i].uri.len, idx->dlist[i].uri.s);
					return -1;
				}
				avp_val.s = idx->dlist[i].attrs.duid;
				if(add_avp(AVP_VAL_STR | dstid_avp_type, dstid_avp_name,
						   avp_val)
						!= 0)
					return -1;
			}
			cnt++;
		}

		lhash = hash;
		/* if addr was not set to dst/uri, add it via last operation */
		if(mode == 2)
			lhash--;
		for(i = idx->nr - 1; i > lhash && cnt < limit; i--) {
			if(ds_skip_dst(idx->dlist[i].flags)
					|| (ds_use_default != 0 && i == (idx->nr - 1)))
				continue;
			/* max load exceeded per destination */
			if(alg == DS_ALG_CALLLOAD
					&& idx->dlist[i].dload >= idx->dlist[i].attrs.maxload)
				LM_DBG("using entry [%d/%d]\n", set, i);
			avp_val.s = idx->dlist[i].uri;
			if(add_avp(AVP_VAL_STR | dst_avp_type, dst_avp_name, avp_val) != 0)
				return -1;

			if(attrs_avp_name.n != 0 && idx->dlist[i].attrs.body.len > 0) {
				avp_val.s = idx->dlist[i].attrs.body;
				if(add_avp(AVP_VAL_STR | attrs_avp_type, attrs_avp_name,
						   avp_val)
						!= 0)
					return -1;
			}

			if(sock_avp_name.n != 0 && idx->dlist[i].sock) {
				avp_val.s.len = 1 + sprintf(buf, "%p", idx->dlist[i].sock);
				avp_val.s.s = buf;
				if(add_avp(AVP_VAL_STR | sock_avp_type, sock_avp_name, avp_val)
						!= 0)
					return -1;
			}

			if(alg == DS_ALG_CALLLOAD) {
				if(idx->dlist[i].attrs.duid.len <= 0) {
					LM_ERR("no uid for destination: %d %.*s\n", set,
							idx->dlist[i].uri.len, idx->dlist[i].uri.s);
					return -1;
				}
				avp_val.s = idx->dlist[i].attrs.duid;
				if(add_avp(AVP_VAL_STR | dstid_avp_type, dstid_avp_name,
						   avp_val)
						!= 0)
					return -1;
			}
			cnt++;
		}

		/* add to avp the first used dst */
		avp_val.s = idx->dlist[hash].uri;
		if(add_avp(AVP_VAL_STR | dst_avp_type, dst_avp_name, avp_val) != 0)
			return -1;

		if(attrs_avp_name.n != 0 && idx->dlist[hash].attrs.body.len > 0) {
			avp_val.s = idx->dlist[hash].attrs.body;
			if(add_avp(AVP_VAL_STR | attrs_avp_type, attrs_avp_name, avp_val)
					!= 0)
				return -1;
		}
		if(sock_avp_name.n != 0 && idx->dlist[hash].sock) {
			avp_val.s.len = 1 + sprintf(buf, "%p", idx->dlist[hash].sock);
			avp_val.s.s = buf;
			if(add_avp(AVP_VAL_STR | sock_avp_type, sock_avp_name, avp_val)
					!= 0)
				return -1;
		}

		if(alg == DS_ALG_CALLLOAD) {
			if(idx->dlist[hash].attrs.duid.len <= 0) {
				LM_ERR("no uid for destination: %d %.*s\n", set,
						idx->dlist[hash].uri.len, idx->dlist[hash].uri.s);
				return -1;
			}
			avp_val.s = idx->dlist[hash].attrs.duid;
			if(add_avp(AVP_VAL_STR | dstid_avp_type, dstid_avp_name, avp_val)
					!= 0)
				return -1;
		}
		cnt++;
	}

	if(grp_avp_name.n != 0) {
		/* add to avp the group id */
		avp_val.n = set;
		if(add_avp(grp_avp_type, grp_avp_name, avp_val) != 0)
			return -1;
	}

	if(cnt_avp_name.n != 0) {
		/* add to avp the number of dst */
		avp_val.n = cnt;
		if(add_avp(cnt_avp_type, cnt_avp_name, avp_val) != 0)
			return -1;
	}

	return 1;
}

int ds_next_dst(struct sip_msg *msg, int mode)
{
	struct search_state st;
	struct usr_avp *avp;
	struct usr_avp *prev_avp;
	struct socket_info *sock = NULL;
	int_str avp_value;
	int_str sock_avp_value;
	int alg = 0;

	if(!(ds_flags & DS_FAILOVER_ON) || dst_avp_name.n == 0) {
		LM_WARN("failover support disabled\n");
		return -1;
	}

	if(dstid_avp_name.n != 0) {
		prev_avp = search_first_avp(
				dstid_avp_type, dstid_avp_name, &avp_value, &st);
		if(prev_avp != NULL) {
			/* load based dispatching */
			alg = DS_ALG_CALLLOAD;
			/* off-load destination id */
			destroy_avp(prev_avp);
		}
	}

	if(attrs_avp_name.n != 0) {
		prev_avp = search_first_avp(
				attrs_avp_type, attrs_avp_name, &avp_value, &st);
		if(prev_avp != NULL) {
			destroy_avp(prev_avp);
		}
	}

	if(sock_avp_name.n != 0) {
		prev_avp = search_first_avp(
				sock_avp_type, sock_avp_name, &sock_avp_value, &st);
		if(prev_avp != NULL) {
			if(sscanf(sock_avp_value.s.s, "%p", (void **)&sock) != 1)
				sock = NULL;
			destroy_avp(prev_avp);
		}
	}

	prev_avp = search_first_avp(dst_avp_type, dst_avp_name, &avp_value, &st);
	if(prev_avp == NULL)
		return -1; /* used avp deleted -- strange */

	avp = search_next_avp(&st, &avp_value);
	destroy_avp(prev_avp);
	if(avp == NULL || !(avp->flags & AVP_VAL_STR))
		return -1; /* no more avps or value is int */

	if(ds_update_dst(msg, &avp_value.s, sock, mode) != 0) {
		LM_ERR("cannot set dst addr\n");
		return -1;
	}
	LM_DBG("using [%.*s]\n", avp_value.s.len, avp_value.s.s);
	if(alg == DS_ALG_CALLLOAD) {
		prev_avp = search_first_avp(
				dstid_avp_type, dstid_avp_name, &avp_value, &st);
		if(prev_avp == NULL) {
			LM_ERR("cannot find uid avp for destination address\n");
			return -1;
		}
		if(ds_load_replace(msg, &avp_value.s) < 0) {
			LM_ERR("cannot update load distribution\n");
			return -1;
		}
	}

	return 1;
}

int ds_mark_dst(struct sip_msg *msg, int state)
{
	int group, ret;
	struct usr_avp *prev_avp;
	int_str avp_value;

	if(!(ds_flags & DS_FAILOVER_ON)) {
		LM_WARN("failover support disabled\n");
		return -1;
	}

	prev_avp = search_first_avp(grp_avp_type, grp_avp_name, &avp_value, 0);

	if(prev_avp == NULL || prev_avp->flags & AVP_VAL_STR)
		return -1; /* grp avp deleted -- strange */
	group = avp_value.n;

	prev_avp = search_first_avp(dst_avp_type, dst_avp_name, &avp_value, 0);

	if(prev_avp == NULL || !(prev_avp->flags & AVP_VAL_STR))
		return -1; /* dst avp deleted -- strange */

	ret = ds_update_state(msg, group, &avp_value.s, state);

	LM_DBG("state [%d] grp [%d] dst [%.*s]\n", state, group, avp_value.s.len,
			avp_value.s.s);

	return (ret == 0) ? 1 : -1;
}

static inline void latency_stats_update(ds_latency_stats_t *latency_stats, int latency) {
	/* after 2^21 ~24 days at 1s interval, the average becomes a weighted average */
	if (latency_stats->count < 2097152) {
		latency_stats->count++;
	} else { /* We adjust the sum of squares used by the oneline algorithm proportionally */
		latency_stats->m2 -= latency_stats->m2/latency_stats->count;
	}
	if (latency_stats->count == 1) {
		latency_stats->stdev = 0.0f;
		latency_stats->m2 = 0.0f;
		latency_stats->max = latency;
		latency_stats->min = latency;
		latency_stats->average = latency;
		latency_stats->estimate = latency;
	}
	if (latency_stats->min > latency)
		latency_stats->min = latency;
	if (latency_stats->max < latency)
		latency_stats->max = latency;

	/* standard deviation using oneline algorithm */
	/* https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online_algorithm */
	if (latency_stats->count > 1) {
		float delta = latency - latency_stats->average;
		latency_stats->average += delta/latency_stats->count;
		float delta2 = latency - latency_stats->average;
		latency_stats->m2 += delta*delta2;
		latency_stats->stdev = sqrt(latency_stats->m2 / (latency_stats->count-1));
	}
	/* exponentialy weighted moving average */
	if (latency_stats->count < 10) {
		latency_stats->estimate = latency_stats->average;
	} else {
		latency_stats->estimate = latency_stats->estimate*ds_latency_estimator_alpha
		                          + latency*(1-ds_latency_estimator_alpha);
	}
}

int ds_update_latency(int group, str *address, int code)
{
	int i = 0;
	int state = 0;
	ds_set_t *idx = NULL;

	if(_ds_list == NULL || _ds_list_nr <= 0) {
		LM_ERR("the list is null\n");
		return -1;
	}

	/* get the index of the set */
	if(ds_get_index(group, *crt_idx, &idx) != 0) {
		LM_ERR("destination set [%d] not found\n", group);
		return -1;
	}

	while(i < idx->nr) {
		if(idx->dlist[i].uri.len == address->len
				&& strncasecmp(idx->dlist[i].uri.s, address->s, address->len)
						   == 0) {

			/* destination address found */
			state = idx->dlist[i].flags;
			ds_latency_stats_t *latency_stats = &idx->dlist[i].latency_stats;
			if (code == 408 && latency_stats->timeout < UINT32_MAX) {
				latency_stats->timeout++;
			} else {
				struct timeval now;
				gettimeofday(&now, NULL);
				int latency_ms = (now.tv_sec - latency_stats->start.tv_sec)*1000
			            + (now.tv_usec - latency_stats->start.tv_usec)/1000;
				latency_stats_update(latency_stats, latency_ms);
				LM_DBG("[%d]latency[%d]avg[%.2f][%.*s]code[%d]\n", latency_stats->count, latency_ms,
					 latency_stats->average, address->len, address->s, code);
			}
		}
		i++;
	}
	return state;
}


/**
 * Get state for given destination
 */
int ds_get_state(int group, str *address)
{
	int i = 0;
	int state = 0;
	ds_set_t *idx = NULL;

	if(_ds_list == NULL || _ds_list_nr <= 0) {
		LM_ERR("the list is null\n");
		return -1;
	}

	/* get the index of the set */
	if(ds_get_index(group, *crt_idx, &idx) != 0) {
		LM_ERR("destination set [%d] not found\n", group);
		return -1;
	}

	while(i < idx->nr) {
		if(idx->dlist[i].uri.len == address->len
				&& strncasecmp(idx->dlist[i].uri.s, address->s, address->len)
						   == 0) {
			/* destination address found */
			state = idx->dlist[i].flags;
		}
		i++;
	}
	return state;
}

/**
 * Update destionation's state
 */
int ds_update_state(sip_msg_t *msg, int group, str *address, int state)
{
	int i = 0;
	int old_state = 0;
	int init_state = 0;
	ds_set_t *idx = NULL;

	if(_ds_list == NULL || _ds_list_nr <= 0) {
		LM_ERR("the list is null\n");
		return -1;
	}

	/* get the index of the set */
	if(ds_get_index(group, *crt_idx, &idx) != 0) {
		LM_ERR("destination set [%d] not found\n", group);
		return -1;
	}

	while(i < idx->nr) {
		if(idx->dlist[i].uri.len == address->len
				&& strncasecmp(idx->dlist[i].uri.s, address->s, address->len)
						   == 0) {
			/* destination address found */
			old_state = idx->dlist[i].flags;

			/* reset the bits used for states */
			idx->dlist[i].flags &= ~(DS_STATES_ALL);

			/* we need the initial state for inactive counter */
			init_state = state;

			if((state & DS_TRYING_DST) && (old_state & DS_INACTIVE_DST)) {
				/* old state is inactive, new state is trying => keep it inactive
				 * - it has to go first to active state and then to trying */
				state &= ~(DS_TRYING_DST);
				state |= DS_INACTIVE_DST;
			}

			/* set the new states */
			if(state & DS_DISABLED_DST) {
				idx->dlist[i].flags |= DS_DISABLED_DST;
			} else {
				idx->dlist[i].flags |= state;
			}

			if(state & DS_TRYING_DST) {
				idx->dlist[i].message_count++;
				/* Destination is not replying.. Increasing failure counter */
				if(idx->dlist[i].message_count >= probing_threshold) {
					/* Destionation has too much lost messages.. Bringing it to inactive state */
					idx->dlist[i].flags &= ~DS_TRYING_DST;
					idx->dlist[i].flags |= DS_INACTIVE_DST;
					idx->dlist[i].message_count = 0;
				}
			} else {
				if(!(init_state & DS_TRYING_DST)
						&& (old_state & DS_INACTIVE_DST)) {
					idx->dlist[i].message_count++;
					/* Destination was inactive but it is just replying.. Increasing successful counter */
					if(idx->dlist[i].message_count < inactive_threshold) {
						/* Destination has not enough successful replies.. Leaving it into inactive state */
						idx->dlist[i].flags |= DS_INACTIVE_DST;
					} else {
						/* Destination has enough replied messages.. Bringing it to active state */
						idx->dlist[i].message_count = 0;
					}
				} else {
					idx->dlist[i].message_count = 0;
				}
			}

			if(!ds_skip_dst(old_state) && ds_skip_dst(idx->dlist[i].flags)) {
				ds_run_route(msg, address, "dispatcher:dst-down");

			} else {
				if(ds_skip_dst(old_state) && !ds_skip_dst(idx->dlist[i].flags))
					ds_run_route(msg, address, "dispatcher:dst-up");
			}
			if(idx->dlist[i].attrs.rweight > 0)
				ds_reinit_rweight_on_state_change(
						old_state, idx->dlist[i].flags, idx);

			return 0;
		}
		i++;
	}

	return -1;
}

static void ds_run_route(sip_msg_t *msg, str *uri, char *route)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng = NULL;
	str evname;

	if(route == NULL) {
		LM_ERR("bad route\n");
		return;
	}

	LM_DBG("ds_run_route event_route[%s]\n", route);

	rt = -1;
	if(ds_event_callback.s==NULL || ds_event_callback.len<=0) {
		rt = route_lookup(&event_rt, route);
		if(rt < 0 || event_rt.rlist[rt] == NULL) {
			LM_DBG("route does not exist");
			return;
		}
	} else {
		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			LM_DBG("event callback (%s) set, but no cfg engine\n",
					ds_event_callback.s);
			return;
		}
	}

	if(msg == NULL) {
		if(faked_msg_init() < 0) {
			LM_ERR("faked_msg_init() failed\n");
			return;
		}
		fmsg = faked_msg_next();
		fmsg->parsed_orig_ruri_ok = 0;
		fmsg->new_uri = *uri;
	} else {
		fmsg = msg;
	}

	if(rt>=0 || ds_event_callback.len>0) {
		backup_rt = get_route_type();
		set_route_type(REQUEST_ROUTE);
		init_run_actions_ctx(&ctx);
		if(rt>=0) {
			run_top_route(event_rt.rlist[rt], fmsg, 0);
		} else {
			if(keng!=NULL) {
				evname.s = route;
				evname.len = strlen(evname.s);
				if(keng->froute(fmsg, EVENT_ROUTE,
							&ds_event_callback, &evname)<0) {
					LM_ERR("error running event route kemi callback\n");
				}
			}
		}
		set_route_type(backup_rt);
	}
}


/**
 * recalculate relative states if some destination state was changed
 */
int ds_reinit_rweight_on_state_change(
		int old_state, int new_state, ds_set_t *dset)
{
	if(dset == NULL) {
		LM_ERR("destination set is null\n");
		return -1;
	}
	if((!ds_skip_dst(old_state) && ds_skip_dst(new_state))
			|| (ds_skip_dst(old_state) && !ds_skip_dst(new_state))) {
		dp_init_relative_weights(dset);
	}

	return 0;
}


/**
 *
 */
int ds_reinit_state(int group, str *address, int state)
{
	int i = 0;
	ds_set_t *idx = NULL;

	if(_ds_list == NULL || _ds_list_nr <= 0) {
		LM_ERR("the list is null\n");
		return -1;
	}

	/* get the index of the set */
	if(ds_get_index(group, *crt_idx, &idx) != 0) {
		LM_ERR("destination set [%d] not found\n", group);
		return -1;
	}

	for(i = 0; i < idx->nr; i++) {
		if(idx->dlist[i].uri.len == address->len
				&& strncasecmp(idx->dlist[i].uri.s, address->s, address->len)
						   == 0) {
			int old_state = idx->dlist[i].flags;
			/* reset the bits used for states */
			idx->dlist[i].flags &= ~(DS_STATES_ALL);
			/* set the new states */
			idx->dlist[i].flags |= state;
			if(idx->dlist[i].attrs.rweight > 0) {
				ds_reinit_rweight_on_state_change(
						old_state, idx->dlist[i].flags, idx);
			}

			return 0;
		}
	}
	LM_ERR("destination address [%d : %.*s] not found\n", group, address->len,
			address->s);
	return -1;
}

/**
 *
 */
void ds_fprint_set(FILE *fout, ds_set_t *node)
{
	int i, j;

	if(!node)
		return;

	for(i = 0; i < 2; ++i)
		ds_fprint_set(fout, node->next[i]);

	for(j = 0; j < node->nr; j++) {
		fprintf(fout, "\n set #%d\n", node->id);

		if(node->dlist[j].flags & DS_DISABLED_DST)
			fprintf(fout, "    Disabled         ");
		else if(node->dlist[j].flags & DS_INACTIVE_DST)
			fprintf(fout, "    Inactive         ");
		else if(node->dlist[j].flags & DS_TRYING_DST) {
			fprintf(fout, "    Trying");
			/* print the tries for this host. */
			if(node->dlist[j].message_count > 0) {
				fprintf(fout, " (Fail %d/%d)", node->dlist[j].message_count,
						probing_threshold);
			} else {
				fprintf(fout, "           ");
			}

		} else {
			fprintf(fout, "    Active           ");
		}
		if(node->dlist[j].flags & DS_PROBING_DST)
			fprintf(fout, "(P)");
		else
			fprintf(fout, "(*)");

		fprintf(fout, "   %.*s\n", node->dlist[j].uri.len,
				node->dlist[j].uri.s);
	}
}

/**
 *
 */
int ds_fprint_list(FILE *fout)
{
	if(_ds_list == NULL || _ds_list_nr <= 0) {
		LM_ERR("no destination sets\n");
		return -1;
	}

	fprintf(fout, "\nnumber of destination sets: %d\n", _ds_list_nr);

	ds_fprint_set(fout, _ds_list);

	return 0;
}


int ds_is_addr_from_set(sip_msg_t *_m, struct ip_addr *pipaddr,
		unsigned short tport, unsigned short tproto, ds_set_t *node, int mode,
		int export_set_pv)
{
	pv_value_t val;
	int j;
	for(j = 0; j < node->nr; j++) {
		if(ip_addr_cmp(pipaddr, &node->dlist[j].ip_address)
				&& ((mode & DS_MATCH_NOPORT) || node->dlist[j].port == 0
						   || tport == node->dlist[j].port)
				&& ((mode & DS_MATCH_NOPROTO)
						   || tproto == node->dlist[j].proto)) {
			if(export_set_pv && ds_setid_pvname.s != 0) {
				memset(&val, 0, sizeof(pv_value_t));
				val.flags = PV_VAL_INT | PV_TYPE_INT;

				val.ri = node->id;
				if(ds_setid_pv.setf(_m, &ds_setid_pv.pvp, (int)EQ_T, &val)
						< 0) {
					LM_ERR("setting PV failed\n");
					return -2;
				}
			}
			if(ds_attrs_pvname.s != 0 && node->dlist[j].attrs.body.len > 0) {
				memset(&val, 0, sizeof(pv_value_t));
				val.flags = PV_VAL_STR;
				val.rs = node->dlist[j].attrs.body;
				if(ds_attrs_pv.setf(_m, &ds_attrs_pv.pvp, (int)EQ_T, &val)
						< 0) {
					LM_ERR("setting attrs pv failed\n");
					return -3;
				}
			}
			return 1;
		}
	}
	return -1;
}

/**
 *
 */
int ds_is_addr_from_set_r(sip_msg_t *_m, struct ip_addr *pipaddr,
		unsigned short tport, unsigned short tproto, ds_set_t *node, int mode,
		int export_set_pv)
{
	int i, rc;

	if(!node)
		return -1;

	for(i = 0; i < 2; ++i) {
		rc = ds_is_addr_from_set_r(
				_m, pipaddr, tport, tproto, node->next[i], mode, export_set_pv);
		if(rc != -1)
			return rc;
	}

	return ds_is_addr_from_set(
			_m, pipaddr, tport, tproto, node, mode, export_set_pv);
}

/* Checks, if the request (sip_msg *_m) comes from a host in a group
 * (group-id or -1 for all groups)
 */
int ds_is_addr_from_list(sip_msg_t *_m, int group, str *uri, int mode)
{
	ds_set_t *list;

	struct ip_addr *pipaddr;
	struct ip_addr aipaddr;
	unsigned short tport;
	unsigned short tproto;
	sip_uri_t puri;
	static char hn[256];
	struct hostent *he;
	int rc = -1;

	if(uri == NULL || uri->len <= 0) {
		pipaddr = &_m->rcv.src_ip;
		tport = _m->rcv.src_port;
		tproto = _m->rcv.proto;
	} else {
		if(parse_uri(uri->s, uri->len, &puri) != 0 || puri.host.len > 255) {
			LM_ERR("bad uri [%.*s]\n", uri->len, uri->s);
			return -1;
		}
		strncpy(hn, puri.host.s, puri.host.len);
		hn[puri.host.len] = '\0';

		he = resolvehost(hn);
		if(he == 0) {
			LM_ERR("could not resolve %.*s\n", puri.host.len, puri.host.s);
			return -1;
		}
		hostent2ip_addr(&aipaddr, he, 0);
		pipaddr = &aipaddr;
		tport = puri.port_no;
		tproto = puri.proto;
	}


	if(group == -1) {
		rc = ds_is_addr_from_set_r(
				_m, pipaddr, tport, tproto, _ds_list, mode, 1);
	} else {
		list = ds_avl_find(_ds_list, group);
		if(list) {
			rc = ds_is_addr_from_set(_m, pipaddr, tport, tproto, list, mode, 0);
		}
	}

	return rc;
}

int ds_is_from_list(struct sip_msg *_m, int group)
{
	return ds_is_addr_from_list(_m, group, NULL, DS_MATCH_NOPROTO);
}

/*! \brief
 * Callback-Function for the OPTIONS-Request
 * This Function is called, as soon as the Transaction is finished
 * (e. g. a Response came in, the timeout was hit, ...)
 */
static void ds_options_callback(
		struct cell *t, int type, struct tmcb_params *ps)
{
	int group = 0;
	str uri = {0, 0};
	sip_msg_t *fmsg;
	int state;

	/* The param contains the group, in which the failed host
	 * can be found.*/
	if(ps->param == NULL) {
		LM_DBG("No parameter provided, OPTIONS-Request was finished"
			   " with code %d\n",
				ps->code);
		return;
	}

	fmsg = NULL;

	/* The param is a (void*) Pointer, so we need to dereference it and
	 *  cast it to an int. */
	group = (int)(long)(*ps->param);
	/* The SIP-URI is taken from the Transaction.
	 * Remove the "To: <" (s+5) and the trailing >+new-line (s - 5 (To: <)
	 * - 3 (>\r\n)). */
	uri.s = t->to.s + 5;
	uri.len = t->to.len - 8;
	LM_DBG("OPTIONS-Request was finished with code %d (to %.*s, group %d)\n",
			ps->code, uri.len, uri.s, group);
	if (ds_ping_latency_stats)
		ds_update_latency(group, &uri, ps->code);
	/* ps->code contains the result-code of the request.
	 *
	 * We accept both a "200 OK" or the configured reply as a valid response */
	if((ps->code >= 200 && ps->code <= 299)
			|| ds_ping_check_rplcode(ps->code)) {
		/* Set the according entry back to "Active" */
		state = 0;
		if(ds_probing_mode == DS_PROBE_ALL
				|| ((ds_probing_mode == DS_PROBE_ONLYFLAGGED)
						   && (ds_get_state(group, &uri) & DS_PROBING_DST)))
			state |= DS_PROBING_DST;

		/* Check if in the meantime someone disabled the target through RPC or MI */
		if(!(ds_get_state(group, &uri) & DS_DISABLED_DST)
				&& ds_update_state(fmsg, group, &uri, state) != 0) {
			LM_ERR("Setting the state failed (%.*s, group %d)\n", uri.len,
					uri.s, group);
		}
	} else {
		state = DS_TRYING_DST;
		if(ds_probing_mode != DS_PROBE_NONE)
			state |= DS_PROBING_DST;
		/* Check if in the meantime someone disabled the target through RPC or MI */
		if(!(ds_get_state(group, &uri) & DS_DISABLED_DST)
				&& ds_update_state(fmsg, group, &uri, state) != 0) {
			LM_ERR("Setting the probing state failed (%.*s, group %d)\n",
					uri.len, uri.s, group);
		}
	}

	return;
}

/**
 *
 */
void ds_ping_set(ds_set_t *node)
{
	uac_req_t uac_r;
	int i, j;

	if(!node)
		return;

	for(i = 0; i < 2; ++i)
		ds_ping_set(node->next[i]);

	for(j = 0; j < node->nr; j++) {
		/* skip addresses set in disabled state by admin */
		if((node->dlist[j].flags & DS_DISABLED_DST) != 0)
			continue;
		/* If the Flag of the entry has "Probing set, send a probe:	*/
		if(ds_probing_mode == DS_PROBE_ALL
				|| (node->dlist[j].flags & DS_PROBING_DST) != 0) {
			LM_DBG("probing set #%d, URI %.*s\n", node->id,
					node->dlist[j].uri.len, node->dlist[j].uri.s);

			/* Send ping using TM-Module.
			 * int request(str* m, str* ruri, str* to, str* from, str* h,
			 *		str* b, str *oburi,
			 *		transaction_cb cb, void* cbp); */
			set_uac_req(&uac_r, &ds_ping_method, 0, 0, 0, TMCB_LOCAL_COMPLETED,
					ds_options_callback, (void *)(long)node->id);
			if(node->dlist[j].attrs.socket.s != NULL
					&& node->dlist[j].attrs.socket.len > 0) {
				uac_r.ssock = &node->dlist[j].attrs.socket;
			} else if(ds_default_socket.s != NULL
					  && ds_default_socket.len > 0) {
				uac_r.ssock = &ds_default_socket;
			}

			gettimeofday(&node->dlist[j].latency_stats.start, NULL);

			if(tmb.t_request(&uac_r, &node->dlist[j].uri, &node->dlist[j].uri,
					   &ds_ping_from, &ds_outbound_proxy)
					< 0) {
				LM_ERR("unable to ping [%.*s]\n", node->dlist[j].uri.len,
						node->dlist[j].uri.s);
			}
		}
	}
}

/*! \brief
 * Timer for checking probing destinations
 *
 * This timer is regularly fired.
 */
void ds_check_timer(unsigned int ticks, void *param)
{


	/* Check for the list. */
	if(_ds_list == NULL || _ds_list_nr <= 0) {
		LM_DBG("no destination sets\n");
		return;
	}

	if(_ds_ping_active != NULL && *_ds_ping_active == 0) {
		LM_DBG("pinging destinations is inactive by admin\n");
		return;
	}

	ds_ping_set(_ds_list);
}

/*! \brief
 * Timer for checking expired items in call load dispatching
 *
 * This timer is regularly fired.
 */
void ds_ht_timer(unsigned int ticks, void *param)
{
	ds_cell_t *it;
	ds_cell_t *it0;
	time_t now;
	int i;

	if(_dsht_load == NULL)
		return;

	now = time(NULL);

	for(i = 0; i < _dsht_load->htsize; i++) {
		/* free entries */
		lock_get(&_dsht_load->entries[i].lock);
		it = _dsht_load->entries[i].first;
		while(it) {
			it0 = it->next;
			if((it->expire != 0 && it->expire < now)
					|| (it->state == DS_LOAD_INIT && it->initexpire != 0
							   && it->initexpire < now)) {
				/* expired */
				if(it->prev == NULL)
					_dsht_load->entries[i].first = it->next;
				else
					it->prev->next = it->next;
				if(it->next)
					it->next->prev = it->prev;
				_dsht_load->entries[i].esize--;

				/* execute ds unload callback */
				ds_load_remove_byid(it->dset, &it->duid);

				ds_cell_free(it);
			}
			it = it0;
		}
		lock_release(&_dsht_load->entries[i].lock);
	}
	return;
}

int bind_dispatcher(dispatcher_api_t *api)
{
	if(!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->select = ds_select_dst;
	api->next = ds_next_dst;
	api->mark = ds_mark_dst;
	api->is_from = ds_is_from_list;
	return 0;
}


ds_set_t *ds_get_list(void)
{
	return _ds_list;
}

int ds_get_list_nr(void)
{
	return _ds_list_nr;
}

ds_set_t *ds_avl_find(ds_set_t *node, int id)
{
	int next_step;

	while(node && id != node->id) {
		next_step = (id > node->id);
		node = node->next[next_step];
	}
	return node;
}

/**
 *
 */
void ds_avl_destroy(ds_set_t **node_ptr)
{
	ds_set_t *node = NULL;
	ds_dest_t *dest = NULL;
	int i = 0;

	if(!node_ptr || !(*node_ptr))
		return;

	node = *node_ptr;

	for(i = 0; i < 2; ++i)
		ds_avl_destroy(&node->next[i]);

	for(dest = node->dlist; dest != NULL; dest = dest->next) {
		if(dest->uri.s != NULL) {
			shm_free(dest->uri.s);
			dest->uri.s = NULL;
		}
	}
	if(node->dlist != NULL)
		shm_free(node->dlist);
	shm_free(node);

	*node_ptr = NULL;

	return;
}

static void avl_rebalance(ds_set_t **path_top, int target);

ds_set_t *ds_avl_insert(ds_set_t **root, int id, int *setn)
{
	ds_set_t **rotation_top;
	ds_set_t *node;
	int next_step;

	rotation_top = root;
	node = *root;

	while(node && id != node->id) {
		next_step = (id > node->id);
		if(!AVL_BALANCED(node))
			rotation_top = root;
		root = &node->next[next_step];
		node = *root;
	}
	if(!node) {
		node = shm_malloc(sizeof(ds_set_t));
		memset(node, 0, sizeof(ds_set_t));
		node->id = id;
		node->longer = AVL_NEITHER;
		*root = node;

		avl_rebalance(rotation_top, id);

		(*setn)++;
	}
	return node;
}

static void avl_rebalance_path(ds_set_t *path, int id)
{
	int next_step;
	/* Each node in path is currently balanced.
	 * Until we find target, mark each node as longer
	 * in the direction of target because we know we have
	 * inserted target there
	 */
	while(path && id != path->id) {
		next_step = (id > path->id);
		path->longer = next_step;
		path = path->next[next_step];
	}
}

static ds_set_t *avl_rotate_2(ds_set_t **path_top, int dir)
{
	ds_set_t *B, *C, *D, *E;

	B = *path_top;
	D = B->next[dir];
	C = D->next[1 - dir];
	E = D->next[dir];
	*path_top = D;
	D->next[1 - dir] = B;
	B->next[dir] = C;
	B->longer = AVL_NEITHER;
	D->longer = AVL_NEITHER;

	return E;
}

static ds_set_t *avl_rotate_3(ds_set_t **path_top, int dir, int third)
{
	ds_set_t *B, *F, *D, *C, *E;

	B = *path_top;
	F = B->next[dir];
	D = F->next[1 - dir];
	/* node: C and E can be NULL */
	C = D->next[1 - dir];
	E = D->next[dir];
	*path_top = D;
	D->next[1 - dir] = B;
	D->next[dir] = F;
	B->next[dir] = C;
	F->next[1 - dir] = E;
	D->longer = AVL_NEITHER;

	/* assume both trees are balanced */
	B->longer = F->longer = AVL_NEITHER;

	if(third == AVL_NEITHER)
		return NULL;

	if(third == dir) {
		/* E holds the insertion so B is unbalanced */
		B->longer = 1 - dir;
		return E;
	} else {
		/* C holds the insertion so F is unbalanced */
		F->longer = dir;
		return C;
	}
}

static void avl_rebalance(ds_set_t **path_top, int id)
{
	ds_set_t *path;
	int first, second, third;

	path = *path_top;

	if(AVL_BALANCED(path)) {
		avl_rebalance_path(path, id);
		return;
	}
	first = (id > path->id);
	if(path->longer != first) {
		/* took the shorter path */
		path->longer = AVL_NEITHER;
		avl_rebalance_path(path->next[first], id);
		return;
	}
	/* took the longer path, need to rotate */
	second = (id > path->next[first]->id);
	if(first == second) {
		/* just a two-point rotate */
		path = avl_rotate_2(path_top, first);
		avl_rebalance_path(path, id);
		return;
	}
	/* fine details of the 3 point rotate depend on the third step.
	 * However there may not be a third step, if the third point of the
	 * rotation is the newly inserted point.  In that case we record
	 * the third step as NEITHER
	 */
	path = path->next[first]->next[second];
	if(id == path->id)
		third = AVL_NEITHER;
	else
		third = (id > path->id);
	path = avl_rotate_3(path_top, first, third);
	avl_rebalance_path(path, id);
}
