/* offline presence authorization "requests" e.g. storing
 * watcher information for offline presentities */

#include <stdio.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "dlist.h"
#include "presentity.h"
#include "watcher.h"
#include "pdomain.h"
#include "pa_mod.h"
#include "../../parser/parse_from.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <presence/pidf.h>
#include <cds/logger.h>
#include <time.h>
#include "offline_winfo.h"

#include "winfo_doc.h"
#include "message.h"

/* ----- Helper and internal functions ----- */

#define add_str_len(len,p)	if (p) if (p->s) len += p->len
#define set_member_str(buf,len,dst,src)	if (src) if (src->s) { \
			memcpy(buf + len, src->s, src->len); \
			dst.s = buf + len; \
			dst.len = src->len; \
			len += src->len; \
		}

#define string_val(v,s)	(v).type = DB_STR; \
	(v).val.str_val=s; \
	(v).nul=(s.len == 0); 

#define time_val(v,t)	(v).type = DB_DATETIME; \
	(v).val.time_val=t;\
	(v).nul=0; 

#define get_str_val(rvi,dst)	do{if(!rvi.nul){dst.s=(char*)rvi.val.string_val;dst.len=strlen(dst.s);}}while(0)
#define get_blob_val(rvi,dst)	do{if(!rvi.nul){dst=rvi.val.blob_val;}else dst.len=0;}while(0)
#define get_time_val(rvi,dst)	do{if(!rvi.nul){dst=rvi.val.time_val;}}while(0)
#define get_int_val(rvi,dst)	do{if(!rvi.nul){dst=rvi.val.int_val;}else dst=0;}while(0)

/* expiration time in secs */
int offline_winfo_expiration = 259200;

/* status of last subscription */
static watcher_status_t last_subscription_status = WS_PENDING; 

void set_last_subscription_status(watcher_status_t status)
{
	last_subscription_status = status;
}

watcher_status_t get_last_subscription_status()
{
	return last_subscription_status;
}

static offline_winfo_t *create_winfo(str *uid, 
		str *wuri, 
		str *events, 
		str *domain,
		str *status)
{
	int len = 0;
	offline_winfo_t *info;
	
	add_str_len(len, uid);
	add_str_len(len, wuri);
	add_str_len(len, events);
	add_str_len(len, domain);
	add_str_len(len, status);
	len += sizeof(offline_winfo_t);
	
	info = (offline_winfo_t*)mem_alloc(len);
	if (info) {
		memset(info, 0, len);
		len = 0;
		set_member_str(info->buffer, len, info->uid, uid);
		set_member_str(info->buffer, len, info->watcher, wuri);
		set_member_str(info->buffer, len, info->events, events);
		set_member_str(info->buffer, len, info->domain, domain);
		set_member_str(info->buffer, len, info->status, status);
		info->created = time(NULL);
		info->expires = info->created + offline_winfo_expiration;
		info->index = -1;
	}

	return info;
}

static inline void free_winfo(offline_winfo_t *info)
{
	if (info) mem_free(info);
}

static inline void free_winfos(offline_winfo_t *info)
{
	offline_winfo_t *n;
	
	while (info) {
		n = info->next;
		free_winfo(info);
		info = n;
	}
}

static int db_store_winfo(offline_winfo_t *info)
{
	/* ignore duplicit records (should be stored only once!) */
	db_key_t cols[20];
	db_val_t vals[20];
	int n = -1;
	
	if (!pa_db) {
		ERR("database not initialized: set parameter \'use_offline_winfo\' to 1\n");
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, offline_winfo_table) < 0) {
		LOG(L_ERR, "db_add_watcher: Error in use_table\n");
		return -1;
	}
	
	cols[++n] = col_uid;
	string_val(vals[n], info->uid);
	
	cols[++n] = col_watcher;
	string_val(vals[n], info->watcher);
	
	cols[++n] = col_events;
	string_val(vals[n], info->events);
	
	cols[++n] = col_domain;
	string_val(vals[n], info->domain);
	
	cols[++n] = col_status;
	string_val(vals[n], info->status);
	
	cols[++n] = col_created_on;
	time_val(vals[n], info->created);
	
	cols[++n] = col_expires_on;
	time_val(vals[n], info->expires);

	/* index ("dbid") is created automaticaly ! */
	
	/* insert new record into database */
	if (pa_dbf.insert(pa_db, cols, vals, n + 1) < 0) {
		return -1;
	}

	return 0;
}

static int get_watcher_uri(struct sip_msg* _m, str* uri)
{
	struct sip_uri puri;
	int res = 0;
	
	uri->s = get_from(_m)->uri.s;
	uri->len = get_from(_m)->uri.len;

	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LOG(L_ERR, "Error while parsing URI\n");
		return -1;
	}
#if 0	
	uri->s = puri.user.s;
	if ((!uri->s) || (puri.user.len < 1)) {
		uri->s = puri.host.s;
		uri->len = puri.host.len;
		res = 1; /* it is uri without username ! */
	}
#endif
	uri->len = puri.host.s + puri.host.len - uri->s;
	return res;
}

static int get_events(struct sip_msg* _m, str* events)
{
	char *c;
	str_clear(events);
	
	if (parse_headers(_m, HDR_EVENT_F, 0) == -1) {
		ERR("Error while parsing headers\n");
		return -1;
	}
	if (_m->event) {
		/* parse_event(_m->event); */
		*events = _m->event->body;
		c = str_strchr(events, ';');
		if (c) events->len = c - events->s;
	}

	return 0;
}

int remove_expired_winfos()
{
	db_key_t keys[] = { col_expires_on };
	db_val_t vals[1] = { 
		{ DB_DATETIME, 0, { .time_val = time(NULL) } }
	};
	db_op_t ops[] = { OP_LEQ };
	int res = 0;

	if (!pa_db) {
		ERR("database not initialized: set parameter \'use_offline_winfo\' to 1\n");
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, offline_winfo_table) < 0) {
		LOG(L_ERR, "db_add_watcher: Error in use_table\n");
		return -1;
	}

	res = pa_dbf.delete(pa_db, keys, ops, vals, 1);
	if (res < 0 )
		DBG("ERROR cleaning expired offline winfo\n");
	return res;
}

int db_remove_winfos(offline_winfo_t *info)
{
	db_key_t keys[] = { col_dbid };
	db_val_t vals[1];
	db_op_t ops[] = { OP_EQ };
	int res = 0;

	if (!pa_db) {
		ERR("database not initialized: set parameter \'use_offline_winfo\' to 1\n");
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, offline_winfo_table) < 0) {
		LOG(L_ERR, "Error in use_table\n");
		return -1;
	}

	while (info) {
		vals[0].type = DB_INT;
		vals[0].nul = 0;
		vals[0].val.int_val = info->index;

		res = pa_dbf.delete(pa_db, keys, ops, vals, 1);
		if (res < 0 )
			DBG("ERROR cleaning expired offline winfo\n");

		info = info->next;
	}
	return res;
}

static void send_winfo_cb(struct cell* t, int type, struct tmcb_params* params)
{
	offline_winfo_t *info = NULL;

	if (!params) {
		ERR("BUG: empty arg\n");
		return;
	}

	if (params->param) info = (offline_winfo_t *)*(params->param);
	if (!info) {
		ERR("BUG: empty arg\n");
		return;
	}

	if ((params->code >= 200) && (params->code < 300)) {
		/* delete infos from DB */
		db_remove_winfos(info);
	}
	else 
		ERR("%d response on winfo NOTIFY\n", params->code);

	
	/* delete infos from memory */
	free_winfos(info);
}

static int send_winfo(presentity_t *p, offline_winfo_t *info)
{
	watcher_t *w;
	
	if (!p) {
		ERR("BUG: trying to send offline winfo to empty presentity\n");
		return -1;
	}
	
	w = p->first_winfo_watcher;
	while (w) {
		if (w->status == WS_ACTIVE) {
			if (send_winfo_notify_offline(p, w, info, send_winfo_cb, info) == 0) 
				return 0;
		}
		w = w->next;
	}
	
	return -1; /* impossible to send it */
}

int db_load_winfo(str *uid, str *events, str *domain, offline_winfo_t **infos)
{
	int i, r = 0;
	db_res_t *res = NULL;
	db_key_t result_cols[] = { 
		col_watcher, col_created_on, col_expires_on, col_dbid, col_status
	};
	db_key_t keys[] = { col_uid, col_events };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[] = { 
		{ DB_STR, 0, { .str_val = *uid } }
	};
	offline_winfo_t *info = NULL;
	offline_winfo_t *last = NULL;

	*infos = NULL;
	if (pa_dbf.use_table(pa_db, offline_winfo_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (pa_dbf.query (pa_db, keys, ops, k_vals,
			result_cols, 1, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		ERR("Error while querying stored winfos\n");
		r = -1;
		res = NULL;
	}
	if (res) {
		for (i = 0; i < res->n; i++) {
			db_row_t *row = &res->rows[i];
			db_val_t *row_vals = ROW_VALUES(row);
			str watcher = STR_NULL;
			str status = STR_NULL;
			time_t created_on = 0;
			time_t expires_on = 0;
			int index = 0;

			get_str_val(row_vals[0], watcher);
			get_time_val(row_vals[1], created_on);
			get_time_val(row_vals[2], expires_on);
			get_int_val(row_vals[3], index);
			get_str_val(row_vals[4], status);
			
			info = create_winfo(uid, &watcher, events, domain, &status);
			if (!info) { 
				r = -1; 
				break; 
			}
			info->created = created_on;
			info->expires = expires_on;
			info->index = index;
			
			if (last) last->next = info;
			else *infos = info;
			last = info;
		}

		pa_dbf.free_result(pa_db, res);
	}
	if ((*infos) && (r != 0)) {
		free_winfos(*infos);
		*infos = NULL;
	}
	
	return r;
}

/* ----- Handler functions ----- */
#if 0
/* not used due to problems with lost AVP after sending NOTIFY */
static int get_status(str *dst)
{
	avp_t *avp;
	int_str name, val;
	struct search_state s;
	str avp_subscription_status = STR_STATIC_INIT("subscription_status");

	/* if (!dst) return -1; */
	
	name.s = avp_subscription_status;
	avp = search_first_avp(AVP_CLASS_USER | 
			AVP_TRACK_FROM | AVP_NAME_STR | AVP_VAL_STR, name, &val, 0);
	if (avp) {
		/* don't use default - use value from AVP */
		TRACE("subscription status = %.*s\n", FMT_STR(val.s));
		*dst = val.s;
		return 0;
	} 
	else {
		/* leave default value!! */
		/* TRACE("left default subscription status\n"); */
		TRACE("subscription status AVP not found\n");
	}
	
	return 1;
}
#endif

static void get_status_str(str *dst)
{
	str s;
	
	switch(get_last_subscription_status()) {
		case WS_ACTIVE:
			s = watcher_status_names[WS_ACTIVE];
			break;
		case WS_REJECTED:
		case WS_PENDING_TERMINATED:
		case WS_TERMINATED:
			s = watcher_status_names[WS_TERMINATED];
			break;
		case WS_PENDING: 
			s = watcher_status_names[WS_PENDING];
			break;
		default: str_clear(&s); /* throw out gcc complains */
	}
	if (dst) *dst = s;
}

int store_offline_winfo(struct sip_msg* _m, char* _domain, char* _table)
{
	str uid = STR_NULL;
	str wuri = STR_NULL;
	str events = STR_NULL;
	str domain = STR_NULL;
	str status = STR_NULL;
	int res = -1;
	offline_winfo_t *info;

	if (get_presentity_uid(&uid, _m) < 0) {
		ERR("Error while extracting presentity UID\n");
		return 0; /* ??? impossible to return -1 or 1 */
	}
	get_watcher_uri(_m, &wuri);
	get_events(_m, &events);
	if (_domain) {
		domain.s = _domain;
		domain.len = strlen(_domain);
	}

	/* get stored subscription status value */
	get_status_str(&status);
	/* TRACE("subscription status is: %.*s\n", FMT_STR(status)); */

	info = create_winfo(&uid, &wuri, &events, &domain, &status);
	/* store it into database or use internal data structures too? */
	/* better to use only database because of lower memory usage - this 
	 * information could be stored for very long time ! */
	
	db_store_winfo(info);
	
	free_winfo(info); /* don't hold this information in memory ! */

	return res;
}

/* send offline winfo as regular watcher info NOTIFY if possible 
 * (for presentity got from get_to) */
int dump_offline_winfo(struct sip_msg* _m, char* _domain, char* _events)
{
	struct pdomain* d;
	struct presentity *p;
	str uid = STR_NULL;
	int res = -1;
	str events;
	offline_winfo_t *info;

	d = (struct pdomain*)_domain;

	if (get_presentity_uid(&uid, _m) < 0) {
		ERR("Error while extracting presentity UID\n");
		return -1;
	}
	
	if (_events) {
		events.s = _events;
		events.len = strlen(_events);
	}

	if (db_load_winfo(&uid, &events, d->name, &info) != 0) {
		return -1;
	}

	if (!info) {
		return 1; /* nothing to do */
	}

	lock_pdomain(d);

	if (find_presentity_uid(d, &uid, &p) == 0) {
		/* presentity found */
		if (send_winfo(p, info) == 0) res = 1;
		else res = -1;
	}

	unlock_pdomain(d);

	return res;
}

void offline_winfo_timer(unsigned int ticks, void* param)
{
	remove_expired_winfos();
}

int check_subscription_status(struct sip_msg* _m, char* _status, char* _x)
{
	watcher_status_t status = (watcher_status_t)_status;
	if (status == get_last_subscription_status()) return 1;
	else return -1;
}

/* convert char* parameter to watcher_status_t */
int check_subscription_status_fix(void **param, int param_no)
{
	watcher_status_t status = WS_PENDING;
	char *s;
	str ss;

	if (param_no == 1) {
		s = (char*)*param;
		if (!s) {
			ERR("status not given!\n");
			return -1;
		}
		
		/* TRACE("status name is %s\n", (char*)*param); */
		
		ss.s = s;
		ss.len = strlen(s);
		
		status = watcher_status_from_string(&ss);
		*param = (void*)status;
	}
	return 0;
}

