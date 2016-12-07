#include "presentity.h"
#include "pa_mod.h"
#include "ptime.h"
#include "notify.h"
#include "async_auth.h"
#include "tuple.h"
#include "pres_notes.h"
#include "extension_elements.h"

static void process_watchers(presentity_t* _p, int *changed)
{
	watcher_t *next, *w;
	int presentity_changed;
	int notify;
	
	/* !!! "changed" is not initialized here it is only set if change
	 * in presentity occurs */
	
	presentity_changed = _p->flags & PFLAG_PRESENCE_CHANGED;

	w = _p->first_watcher;
	while (w) {
		/* changes status of expired watcher */
		if (w->expires <= act_time) {
			LOG(L_DBG, "Expired watcher %.*s\n", w->uri.len, w->uri.s);
			w->expires = 0;
			set_watcher_terminated_status(w);
			_p->flags |= PFLAG_WATCHERINFO_CHANGED;
			w->flags |= WFLAG_SUBSCRIPTION_CHANGED;
			if (changed) *changed = 1;
		}

		/* send NOTIFY if needed */
		notify = 0;
		if ((w->flags & WFLAG_SUBSCRIPTION_CHANGED)) {
			notify = 1;
			if (changed) *changed = 1; /* ??? */
		}
		if (presentity_changed && is_watcher_authorized(w)) notify = 1;
		if (notify) send_notify(_p, w);
		w->flags &= ~WFLAG_SUBSCRIPTION_CHANGED;
		
		if (is_watcher_terminated(w)) {
			next = w->next;
			remove_watcher(_p, w);
			free_watcher(w);
			w = next;
			if (changed) *changed = 1;
		}
		else w = w->next;
	}
}

static void process_winfo_watchers(presentity_t* _p, int *changed)
{
	watcher_t *next, *w;
	int notify;
	
	/* !!! "changed" is not initialized here it is only set if change
	 * in presentity occurs */
	
	w = _p->first_winfo_watcher;
	while (w) {
		/* changes status of expired watcher */
		if (w->expires <= act_time) {
			LOG(L_DBG, "Expired watcher %.*s\n", w->uri.len, w->uri.s);
			w->expires = 0;
			set_watcher_terminated_status(w);
			w->flags |= WFLAG_SUBSCRIPTION_CHANGED;
			if (changed) *changed = 1;
		}

		/* send NOTIFY if needed */
		notify = 0;
		if ((w->flags & WFLAG_SUBSCRIPTION_CHANGED)) {
			notify = 1;
			if (changed) *changed = 1; /* ??? */
		}
		if ((_p->flags & PFLAG_WATCHERINFO_CHANGED) && 
			is_watcher_authorized(w)) notify = 1;
		if (notify) send_notify(_p, w);
		w->flags &= ~WFLAG_SUBSCRIPTION_CHANGED;
		
		if (is_watcher_terminated(w)) {
			next = w->next;
			remove_watcher(_p, w);
			free_watcher(w);
			w = next;
			if (changed) *changed = 1;
		}
		else w = w->next;
	}
}

/* static void mark_expired_tuples(presentity_t *_p, int *changed)
{
	presence_tuple_t *t;

	t = _p->tuples;
	while (t) {	
		if (t->expires < act_time) {
			t->state = PS_OFFLINE;
			if (changed) *changed = 1;
			_p->flags |= PFLAG_PRESENCE_CHANGED;
		}
		t = t->next;
	}
}*/

static void remove_expired_tuples(presentity_t *_p, int *changed)
{
	presence_tuple_t *t, *n;

	t = (presence_tuple_t*)_p->data.first_tuple;
	while (t) {
		n = (presence_tuple_t *)t->data.next;
		if (t->expires < act_time) {
			DBG("Expiring tuple %.*s\n", t->data.contact.len, t->data.contact.s);
			remove_presence_tuple(_p, t);
			free_presence_tuple(t);
			if (changed) *changed = 1;
			_p->flags |= PFLAG_PRESENCE_CHANGED;
		}
		t = n;
	}
}

static void remove_expired_notes(presentity_t *_p)
{
	pa_presence_note_t *n, *nn;

	n = (pa_presence_note_t*)_p->data.first_note;
	while (n) {
		nn = (pa_presence_note_t *)n->data.next;
		if (n->expires < act_time) {
			DBG("Expiring note %.*s\n", FMT_STR(n->data.value));
			remove_pres_note(_p, n);
			_p->flags |= PFLAG_PRESENCE_CHANGED;
		}
		n = nn;
	}
}

static void remove_expired_extension_elements(presentity_t *_p)
{
	pa_extension_element_t *n, *nn;

	n = (pa_extension_element_t *)_p->data.first_unknown_element;
	while (n) {
		nn = (pa_extension_element_t *)n->data.next;
		if (n->expires < act_time) {
			DBG("Expiring person element %.*s\n", FMT_STR(n->dbid));
			remove_extension_element(_p, n);
			_p->flags |= PFLAG_PRESENCE_CHANGED;
		}
		n = nn;
	}
}

static inline int refresh_auth_rules(presentity_t *p)
{
	/* TODO reload authorization rules if needed */
	if ((p->auth_rules_refresh_time > 0) && 
			(p->auth_rules_refresh_time <= act_time)) {
/*		INFO("refreshing auth rules\n"); */
		ask_auth_rules(p); /* it will run next time if fails now */
		p->auth_rules_refresh_time = act_time + auth_rules_refresh_time;
	}
	return 0;
}

static void process_tuple_change(presentity_t *p, tuple_change_info_t *info)
{
	presence_tuple_t *tuple = NULL;
	basic_tuple_status_t orig;
	time_t e;

	DBG("processing tuple change message: %.*s, %.*s, %d\n",
			FMT_STR(info->user), FMT_STR(info->contact), info->state);

	if (is_str_empty(&info->contact)) {
		/* error - registered tuples need contact address */
		ERR("invalid registered tuple (empty contact)\n");
		return;
	}
	
	if (info->state == presence_tuple_closed) {
		e = act_time + 2 * timer_interval;
	}
	else {
		e = INT_MAX; /* act_time + default_expires; */
		/* hack - re-registrations don't call the callback */
	}
	
	/* Find only registered (not published) tuple - don't overwrite
	 * published information! */
	if (find_registered_presence_tuple(&info->contact, p, &tuple) != 0) {
		/* not found -> create new tuple */
		new_presence_tuple(&info->contact, e, &tuple, 0, NULL, NULL, NULL);
		if (!tuple) return; /* error */
		
		tuple->data.status.basic = info->state;
		add_presence_tuple(p, tuple);
		p->flags |= PFLAG_PRESENCE_CHANGED;
	}
	else {
		/* tuple found -> update */
		orig = tuple->data.status.basic;
		tuple->data.status.basic = info->state;
		tuple->expires = e;
		db_update_presence_tuple(p, tuple, 0);
			
		if (orig != tuple->data.status.basic) p->flags |= PFLAG_PRESENCE_CHANGED;
	}
}

static int process_qsa_message(presentity_t *p, client_notify_info_t *info)
{
	TRACE("received QSA notification for presentity %.*s\n", FMT_STR(p->data.uri));

	/* TODO: handle it as publish for special tuple (but handle merging 
	 * from multiple QSA sources in any way) */
	
	return 0;
}

static void process_presentity_messages(presentity_t *p)
{
	mq_message_t *msg;
	tuple_change_info_t *info;
	client_notify_info_t *qsa_info;

	while ((msg = pop_message(&p->mq)) != NULL) {

		/* FIXME: ugly data type detection */
		if (msg->destroy_function == (destroy_function_f)free_tuple_change_info_content) {
			info = (tuple_change_info_t*)get_message_data(msg);
			if (info) process_tuple_change(p, info);
		}
		else {
			/* QSA message */
			qsa_info = (client_notify_info_t *)get_message_data(msg);
			if (qsa_info) process_qsa_message(p, qsa_info);
		}
			
		free_message(msg);
	}
}


int timer_presentity(presentity_t* _p)
{
	int old_flags;
	int presentity_changed;

	PROF_START(pa_timer_presentity)
	old_flags = _p->flags;

	/* reload authorization rules if needed */
	refresh_auth_rules(_p);
	
	process_presentity_messages(_p);
	
	remove_expired_tuples(_p, NULL);
	
	remove_expired_notes(_p);
	remove_expired_extension_elements(_p);
	
	/* notify watchers and remove expired */
	process_watchers(_p, NULL);	
	/* notify winfo watchers and remove expired */
	process_winfo_watchers(_p, NULL); 
	
	/* notify internal watchers */
	presentity_changed = _p->flags & PFLAG_PRESENCE_CHANGED;
	if (presentity_changed) {
		/* DBG("presentity %.*s changed\n", _p->uri.len, _p->uri.s); */
		notify_qsa_watchers(_p);
	}

	/* clear presentity "change" flags */
	_p->flags &= ~(PFLAG_PRESENCE_CHANGED | PFLAG_WATCHERINFO_CHANGED);
	
	/* update DB record if something changed - USELESS */
/*	if (changed) {
		db_update_presentity(_p);
	} 
*/	
	PROF_STOP(pa_timer_presentity)
	return 0;
}
