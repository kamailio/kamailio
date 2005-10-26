#include "../../fifo_server.h"
#include "../../error.h"
#include "../../parser/parse_event.h"
#include "pa_mod.h"
#include "pdomain.h"
#include "dlist.h"
#include "fifo.h"
#include <cds/sstr.h>
#include <time.h>
#include "qsa_interface.h"

#define MAX_URI		256
#define MAX_DOMAIN	256

extern dlist_t* root; /* FIXME ugly !!!!! */

static void trace_presentity(presentity_t *p, char *response_file)
{
	watcher_t *w;
	presence_tuple_t *t;
	internal_pa_subscription_t *iw;
	
	fifo_reply(response_file, "* %.*s\n", FMT_STR(p->uri));
	
	fifo_reply(response_file, " - tuples:\n");
	t = p->tuples;
	while (t) {		
		fifo_reply(response_file, "    %.*s contact=\'%.*s\' exp=%u status=%d published=%d\n", 
				FMT_STR(t->id), FMT_STR(t->contact), t->expires - time(NULL),
				(int)t->state, t->is_published);
		t = t->next;
	}
	
	fifo_reply(response_file, " - watchers:\n");
	w = p->watchers;
	while (w) {
		fifo_reply(response_file, "    %.*s status=%d exp=%u\n", 
				FMT_STR(w->uri), (int)w->status, w->expires - time(NULL));
		w = w->next;
	}
	
	fifo_reply(response_file, " - winfo watchers:\n");
	w = p->winfo_watchers;
	while (w) {
		fifo_reply(response_file, "    %.*s status=%d exp=%u\n", 
				FMT_STR(w->uri), (int)w->status, w->expires - time(NULL));
		w = w->next;
	}
	
	fifo_reply(response_file, " - internal watchers:\n");
	iw = p->first_qsa_subscription;
	while (iw) {
		fifo_reply(response_file, "     %.*s %d\n", 
				FMT_STR(iw->subscription->subscriber_id), (int)iw->status);
		iw = iw->next;
	}
}

static void trace_dlist(dlist_t *dl, char *response_file)
{
	presentity_t *p;
	
	if (!dl) return;
	if (!dl->d) return;

	lock_pdomain(dl->d);
	fifo_reply(response_file, "domain %.*s\n", FMT_STR(*(dl->d->name)));
	p = dl->d->first;
	while (p) {
		trace_presentity(p, response_file);
		p = p->next;
	}
	unlock_pdomain(dl->d);
}

static int fifo_pa_trace(FILE *fifo, char *response_file)
{
	int res = 0;
	dlist_t *dl;
	
	dl = root;
	while (dl) {
		trace_dlist(dl, response_file);
		dl = dl->next;
	}
	
	return res;
}

static int grant_watcher(presentity_t *p, watcher_t *w)
{
	int changed = 0;
	switch (w->status) {
		case WS_PENDING:
		case WS_REJECTED:
			w->status = WS_ACTIVE;
			changed = 1;
			break;
		case WS_PENDING_TERMINATED:
			w->status = WS_TERMINATED;
			changed = 1;
			break;
			
		default: break;
	}

	if (changed) {
		w->flags |= WFLAG_SUBSCRIPTION_CHANGED;
		if (w->event_package != EVENT_PRESENCE_WINFO) 
			p->flags |= PFLAG_WATCHERINFO_CHANGED;
	}
	
	return 0;
}

static int grant_internal_watcher(presentity_t *p, internal_pa_subscription_t *w)
{
	int changed = 0;
	switch (w->status) {
		case WS_PENDING:
		case WS_REJECTED:
			w->status = WS_ACTIVE;
			changed = 1;
			break;
		case WS_PENDING_TERMINATED:
			w->status = WS_TERMINATED;
			changed = 1;
			break;
			
		default: break;
	}

	if (changed) {
		/* w->flags |= WFLAG_SUBSCRIPTION_CHANGED; */
		notify_internal_watcher(p, w);
		p->flags |= PFLAG_WATCHERINFO_CHANGED;
	}
	
	return 0;
}

static int grant_watchers(presentity_t *p, str *wuri)
{
	watcher_t *w;
	internal_pa_subscription_t *iw;
	
	w = p->watchers;
	while (w) {
		if (str_case_equals(&w->uri, wuri) == 0) grant_watcher(p, w);
		w = w->next;
	}
	
	iw = p->first_qsa_subscription;
	while (iw) {
		if (str_case_equals(&iw->subscription->subscriber_id, wuri) == 0) 
			grant_internal_watcher(p, iw);
		iw = iw->next;
	}

	return 0;
}

static int fifo_pa_authorize(FILE *fifo, char *response_file)
{
	pdomain_t *d;
	presentity_t *p;
	
	char p_uri[MAX_URI] = "";
	char w_uri[MAX_URI] = "";
	char domain[MAX_DOMAIN] = "";
	int domain_len = 0;
	int res = 0;
	str pstr, wstr;
	
	pstr.s = p_uri;
	wstr.s = w_uri;
	read_line(domain, MAX_DOMAIN, fifo, &domain_len);
	read_line(p_uri, MAX_URI, fifo, &pstr.len);
	read_line(w_uri, MAX_URI, fifo, &wstr.len);
	p_uri[pstr.len] = 0;
	w_uri[wstr.len] = 0;
	domain[domain_len] = 0;

	if (find_pdomain(domain, &d) != 0) {
		fifo_reply(response_file, "400 Unknown domain \'%s\'\n", domain);
		return -1;
	}

	lock_pdomain(d);
	if (find_presentity(d, &pstr, &p) != 0) {
		fifo_reply(response_file, "400 Presentity not found \'%s\'\n", p_uri);
		unlock_pdomain(d);
		return -1;
	}
	grant_watchers(p, &wstr);
	unlock_pdomain(d);
	
	if (res == 0) {
		fifo_reply(response_file, "200 Watcher \'%s\' "
			" for presentity \'%s\' authorized\n",
			w_uri, p_uri);
	}
	return res;
}

int pa_fifo_register()
{
	if (register_fifo_cmd(fifo_pa_trace, "pa_trace", 0) < 0) {
		LOG(L_CRIT, "cannot register fifo pa_trace\n");
		return -1;
	}
	
	if (register_fifo_cmd(fifo_pa_authorize, "pa_authorize", 0) < 0) {
		LOG(L_CRIT, "cannot register fifo pa_authorize\n");
		return -1;
	}
	return 0;
}
