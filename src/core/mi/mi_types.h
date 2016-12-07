/*
 * $Id$
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * mi compatibility wrapper for kamailio
 * It contains only the type definition, needed for loading kamilio modules
 * (used by sr_module.c).
 * Note that MI usage in new modules is opsolete. They should implement RPCs
 * instead. To use MI include lib/kmi/mi.h and link with lib kmi.
 *
 * History:
 * --------
 *  2008-11-17  initial version compatible with kamailio mi/mi.h (andrei)
 *  2010-08-18  remove everything but the  data types definition (andrei)
 */

#ifndef _mi_h_
#define _mi_h_

#include "../str.h"

struct mi_node {
	str value;
	str name;
	struct mi_node *kids;
	struct mi_node *next;
	struct mi_node *last;
	struct mi_attr *attributes;
};


struct mi_handler;

struct mi_root {
	unsigned int       code;
	str                reason;
	struct mi_handler  *async_hdl;
	struct mi_node     node;
};

typedef struct mi_root* (mi_cmd_f)(struct mi_root*, void *param);
typedef int (mi_child_init_f)(void);
typedef void (mi_handler_f)(struct mi_root *, struct mi_handler *, int);

/* FIXME
struct mi_handler {
	mi_handler_f *handler_f;
	void * param;
};
*/


struct mi_export_ {
	char *name;
	mi_cmd_f *cmd;
	unsigned int flags;
	void *param;
	mi_child_init_f *init_f;
};

typedef struct mi_export_ mi_export_t;


#endif /* _mi_h_ */


