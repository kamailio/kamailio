/*
 * pua_urloc module - usrloc pua module
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef PUAUSRLOC_API_H
#define PUAUSRLOC_API_H
#include "../../core/str.h"

typedef int (*pua_set_publish_t)(struct sip_msg*, char *, char *);

typedef struct pua_usrloc_binds {
	pua_set_publish_t pua_set_publish;
} pua_usrloc_api_t;

typedef int (*bind_pua_usrloc_f)(pua_usrloc_api_t*);

int bind_pua_usrloc(struct pua_usrloc_binds*);

inline static int pua_usrloc_load_api(pua_usrloc_api_t *pxb)
{
	bind_pua_usrloc_f bind_pua_usrloc_exports;
	if (!(bind_pua_usrloc_exports = (bind_pua_usrloc_f)find_export("bind_pua_usrloc", 1, 0)))
	{
		LM_ERR("Failed to import bind_pua_usrloc\n");
		return -1;
	}
	return bind_pua_usrloc_exports(pxb);
}

#endif /*PUAUSRLOC_API_H*/
