/*
 * $Id$
 */

#ifndef NOTIFY_H
#define NOTIFY_H

#include "../../str.h"
#include "urecord.h"
#include "../../parser/msg_parser.h"

/* FIXME: Possible race condition - a record pointer will be put in notify_record, domain lock
 * will be released, meanwhile pa module unregisters the callback and contacts will be removed
 * too, then the record will be removed and notify_record will point to an non-existend structure
 */

struct urecord;

typedef enum pres_state {
	PRES_OFFLINE = 0,
	PRES_ONLINE
} pres_state_t;

typedef void (*notcb_t)(str* _user, pres_state_t _p, void* _d);


typedef struct notify_cb {
	notcb_t cb;
	void* data;
	struct notify_cb* next;
} notify_cb_t;


void notify_watchers(struct urecord* _r);

int add_watcher(struct urecord* _r, notcb_t _c, void* _d);

int remove_watcher(struct urecord* _r, notcb_t _c, void* _d);

int register_watcher(str* _d, str* _a, notcb_t _c, void* _data);

int unregister_watcher(str* _d, str* _a, notcb_t _c, void* _data);

int post_script(struct sip_msg* _m, void* param);

#endif /* NOTIFY_H */
