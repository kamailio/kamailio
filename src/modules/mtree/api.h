/*
 * Mtree module API specification
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef _MTREE_API_H_
#define _MTREE_API_H_

#include "../../core/parser/msg_parser.h"

typedef int (*mt_match_f)(sip_msg_t *msg, str *tname, str *tomatch, int mval);

typedef struct mtree_api
{
	mt_match_f mt_match;
	mt_match_f mt_match_value;
	mt_match_f mt_match_values;
} mtree_api_t;

typedef int (*bind_mtree_f)(mtree_api_t *api);

/**
 * @brief Load Mtree API
 */
static inline int mtree_load_api(mtree_api_t *api)
{
	bind_mtree_f bind_mtree;

	bind_mtree = (bind_mtree_f)find_export("bind_mtree", 0, 0);
	if(bind_mtree == 0) {
		LM_ERR("cannot find bind_mtree\n");
		return -1;
	}
	if(bind_mtree(api) < 0) {
		LM_ERR("cannot bind mtree api\n");
		return -1;
	}
	return 0;
}


#endif /* _MTREE_API_H_ */
