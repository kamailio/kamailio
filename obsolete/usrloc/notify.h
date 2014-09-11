/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef NOTIFY_H
#define NOTIFY_H

#include "../../str.h"
#include "urecord.h"
#include "../../parser/msg_parser.h"

/* FIXME: Possible race condition - a record pointer will be put in notify_record, domain lock
 * will be released, meanwhile pa module unregisters the callback and contacts will be removed
 * too, then the record will be removed and notify_record will point to an non-existent structure
 */

struct urecord;

typedef enum pres_state {
	PRES_OFFLINE = 0,
	PRES_ONLINE
} pres_state_t;

typedef void (*notcb_t)(str* uid, str* _contact, pres_state_t _p, void* _d);

typedef int (*register_watcher_t)(str* _f, str* _t, notcb_t _c, void* _data);
typedef int (*unregister_watcher_t)(str* _f, str* _t, notcb_t _c, void* _data);

typedef struct notify_cb {
	notcb_t cb;
	void* data;
	struct notify_cb* next;
} notify_cb_t;


void notify_watchers(struct urecord* _r, ucontact_t *_c, int state);

int add_watcher(struct urecord* _r, notcb_t _c, void* _d);

int remove_watcher(struct urecord* _r, notcb_t _c, void* _d);

int register_watcher(str* _d, str* uid, notcb_t _c, void* _data);

int unregister_watcher(str* _d, str* uid, notcb_t _c, void* _data);

int post_script(struct sip_msg* _m, void* param);

#endif /* NOTIFY_H */
