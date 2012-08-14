/*
 * $Id$
 *
 * Copyright (C) 2012 Crocodile RCS Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef OB_API_H
#define OB_API_H

typedef int (*ob_fn1_t)(int, int, int);
typedef int (*ob_fn2_t)(int, int, int);
typedef int (*ob_fn3_t)(int, int, int);

typedef struct ob_binds {
	ob_fn1_t ob_fn1;
	ob_fn2_t ob_fn2;
	ob_fn3_t ob_fn3;
} ob_api_t;

typedef int (*bind_ob_f)(ob_api_t*);

int bind_ob(struct ob_binds*);

inline static int ob_load_api(ob_api_t *pxb)
{
	bind_ob_f bind_ob_exports;
	if (!(bind_ob_exports = (bind_ob_f)find_export("bind_ob", 1, 0)))
	{
		LM_ERR("Failed to import bind_ob\n");
		return -1;
	}
	return bind_ob_exports(pxb);
}

#endif /* OB_API_H */
