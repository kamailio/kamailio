/*
 * $Id$
 */

#include "notify.h"
#include "../../mem/shm_mem.h"
#include "dlist.h"
#include "udomain.h"


struct urecord* notify_record = 0;


void notify_watchers(struct urecord* _r)
{
	notify_cb_t* n;

	n = _r->watchers;
        while(n) {
		n->cb(&_r->aor, (_r->contacts) ? (PRES_ONLINE) : (PRES_OFFLINE), n->data);
		n = n->next;
	}
}


int add_watcher(struct urecord* _r, notcb_t _c, void* _d)
{
	notify_cb_t* ptr;

	ptr = (notify_cb_t*)shm_malloc(sizeof(notify_cb_t));
	if (ptr == 0) {
		LOG(L_ERR, "add_watcher(): No memory left\n");
		return -1;
	}

	ptr->cb = _c;
	ptr->data = _d;
	ptr->next = _r->watchers;
	_r->watchers = ptr;

	ptr->cb(&_r->aor, (_r->contacts) ? (PRES_ONLINE) : (PRES_OFFLINE), ptr->data);
	return 0;
}


int remove_watcher(struct urecord* _r, notcb_t _c, void* _d)
{
	notify_cb_t* ptr, *prev = 0;;

	ptr = _r->watchers;
	while(ptr) {
		if ((ptr->cb == _c) && (ptr->data == _d)) {
			if (prev) prev->next = ptr->next;
			else _r->watchers = ptr->next;
			return 0;
		}
		prev = ptr;
		ptr = ptr->next;
	}
	return 1;
}


int register_watcher(str* _d, str* _a, notcb_t _c, void* _data)
{
	udomain_t* d;
	urecord_t* r;

	if (find_domain(_d, &d) > 0) {
		LOG(L_ERR, "register_watcher(): Domain \'%.*s\' not found\n", _d->len, _d->s);
		return -1;
	}

	lock_udomain(d);

	if (get_urecord(d, _a, &r) > 0) {
		if (insert_urecord(d, _a, &r) < 0) {
			LOG(L_ERR, "register_watcher(): Error while creating a new record\n");
			return -2;
		}
	}

	if (add_watcher(r, _c, _data) < 0) {
		LOG(L_ERR, "register_watcher(): Error while adding a watcher\n");
		release_urecord(r);
		unlock_udomain(d);
		return -3;
	}

	unlock_udomain(d);

	return 0;
}


int unregister_watcher(str* _d, str* _a, notcb_t _c, void* _data)
{
	udomain_t* d;
	urecord_t* r;

	if (find_domain(_d, &d) > 0) {
		LOG(L_ERR, "unregister_watcher(): Domain \'%.*s\' not found\n", _d->len, _d->s);
		return -1;
	}
	
	lock_udomain(d);
	
	if (get_urecord(d, _a, &r) > 0) {
		DBG("unregister_watcher(): Record not found\n");
		return 0;
	}

	remove_watcher(r, _c, _data);
	release_urecord(r);

	unlock_udomain(d);

	return 0;
}


int post_script(struct sip_msg* _m, void* param)
{
	DBG("usrloc: Post-script callback called\n");

	if (notify_record) {
		notify_watchers(notify_record);
		notify_record = 0;
	}

	return 0;
}
