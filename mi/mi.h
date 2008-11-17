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
 * for now it doesn't do anything, it just a compile helper
 * (obsolete, do not use anymore)
 *
 * History:
 * --------
 *  2008-11-17  initial version compatible with kamailio mi/mi.h (andrei)
 */

#ifndef _mi_h_
#define _mi_h_

#include "../str.h"

#define MI_DUP_NAME   (1<<0)
#define MI_DUP_VALUE  (1<<1)

#define MI_OK_S              "OK"
#define MI_OK_LEN            (sizeof(MI_OK_S)-1)
#define MI_INTERNAL_ERR_S    "Server Internal Error"
#define MI_INTERNAL_ERR_LEN  (sizeof(MI_INTERNAL_ERR_S)-1)
#define MI_MISSING_PARM_S    "Too few or too many arguments"
#define MI_MISSING_PARM_LEN  (sizeof(MI_MISSING_PARM_S)-1)
#define MI_BAD_PARM_S        "Bad parameter"
#define MI_BAD_PARM_LEN      (sizeof(MI_BAD_PARM_S)-1)

#define MI_SSTR(_s)           _s,(sizeof(_s)-1)
#define MI_OK                 MI_OK_S
#define MI_INTERNAL_ERR       MI_INTERNAL_ERR_S
#define MI_MISSING_PARM       MI_MISSING_PARM_S
#define MI_BAD_PARM           MI_BAD_PARM_S



struct mi_attr{
	str name;
	str value;
	struct mi_attr *next;
};


struct mi_node {
	str value;
	str name;
	struct mi_node *kids;
	struct mi_node *next;
	struct mi_node *last;
	struct mi_attr *attributes;
};


struct mi_root {
	unsigned int       code;
	str                reason;
	struct mi_handler  *async_hdl;
	struct mi_node     node;
};

typedef struct mi_root* (mi_cmd_f)(struct mi_root*, void *param);
typedef int (mi_child_init_f)(void);


typedef struct mi_export_ {
	char *name;
	mi_cmd_f *cmd;
	unsigned int flags;
	void *param;
	mi_child_init_f *init_f;
}mi_export_t;


#define init_mi_tree(code, reason, reason_len) 0
#define free_mi_tree(parent)
#define add_mi_node_sibling(node, flags, name, name_len, val, val_len) 0
#define add_mi_node_child(node, flags, name, name_len, val, val_len) 0
#define addf_mi_node_child(node, flags, name, name_len, fmt, ...) 0


#endif /* _mi_h_ */


