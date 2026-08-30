/*
 * Copyright (C) 2026 Andrei Lashchinskii <koorwork+kamailio@gmail.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _TARANTOOL_KEMI_H_
#define _TARANTOOL_KEMI_H_

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct str_s
	{
		char *s;
		int len;
	} str_t;

	int sr_kemi_tarantool_call(
			void *msg, str_t *proc_name, str_t *params_json, str_t *res_dst);
	int sr_kemi_tarantool_eval(
			void *msg, str_t *lua_code, str_t *params_json, str_t *res_dst);

#ifdef __cplusplus
}
#endif

#endif /* _TARANTOOL_KEMI_H_ */
