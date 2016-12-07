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
 */

#ifndef _XP_LIB_H_
#define _XP_LIB_H_

#include "../../parser/msg_parser.h"

typedef int (*item_func_t) (struct sip_msg*, str*, str*, int, int);
typedef void (*item_free_t) (str*);

typedef struct _xl_elog
{
	str text;
	str hparam;
	int hindex;
	int hflags;
	item_func_t itf;
	item_free_t free_f;
	struct _xl_elog *next;
} xl_elog_t, *xl_elog_p;

/* callback function to parse the regular expression
 * back references */
typedef int (*xl_parse_cb) (str *, int, xl_elog_p);

typedef int (xl_elog_free_all_f)(xl_elog_p log);
/* int xl_elog_free_all(xl_elog_p log); */
typedef int (xl_parse_format_f)(char *s, xl_elog_p *el);
//int xl_parse_format(char *s, xl_elog_p *el);
typedef int (xl_parse_format2_f)(char *s, xl_elog_p *el, xl_parse_cb cb);
typedef int (xl_print_log_f)(struct sip_msg*, xl_elog_t*, char*, int*);
//int xl_print_log(struct sip_msg* msg, xl_elog_p log, char *buf, int *len);
typedef str* (xl_get_nulstr_f)(void);

typedef struct xl_api {
	xl_print_log_f		*xprint;
	xl_parse_format_f	*xparse;
	xl_parse_format_f	*shm_xparse;
	xl_parse_format2_f	*xparse2;
	xl_parse_format2_f	*shm_xparse2;
	xl_elog_free_all_f	*xfree;
	xl_elog_free_all_f	*shm_xfree;
	xl_get_nulstr_f		*xnulstr;
} xl_api_t;

typedef int (xl_bind_f)(xl_api_t *xl_api);

/* bind to the xl_lib api */
xl_bind_f xl_bind;
/* parse and free functions that work with pkg memory */
xl_elog_free_all_f xl_elog_free_all;
xl_parse_format_f xl_parse_format;
/* parse and free functions that work with shm memory */
xl_elog_free_all_f xl_elog_shm_free_all;
xl_parse_format_f xl_shm_parse_format;
/* parse function supporting regexp back references - pkg mem version */
xl_parse_format2_f xl_parse_format2;
/* parse function supporting regexp back references - shm mem version */
xl_parse_format2_f xl_shm_parse_format2;


xl_print_log_f xl_print_log;
xl_get_nulstr_f xl_get_nulstr;

int xl_mod_init();
int xl_child_init(int);
#endif

