/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 * Copyright (C) 2025-2026 Michael Furmur (didww.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "param_map_parser.h"
#include "../../core/dprint.h"
#include "log_victorialogs.h"

enum parser_state
{
	PS_BEGIN,
	PS_KEY,
	PS_NAME,
	PS_TYPE,
	PS_END
};

int parse_log_mapping(char *val, parser_cb cb, void *userdata)
{
	enum parser_state ps = PS_BEGIN;
	str key = STR_NULL;
	str name = STR_NULL;
	str type = STR_NULL;
	char *c = val, *begin = 0;
	int match_s = 1;
	int include = 0;

#define CHECK_AND_FILL_PARAMETER                                              \
	if(begin) {                                                               \
		switch(ps) {                                                          \
			case PS_KEY:                                                      \
				key.s = begin;                                                \
				key.len = c - begin;                                          \
				begin = 0;                                                    \
				if(cb(&key, 0, LPT_NONE, 0, userdata) < 0)                    \
					return -1;                                                \
				break;                                                        \
			case PS_NAME:                                                     \
				name.s = begin;                                               \
				name.len = c - begin;                                         \
				begin = 0;                                                    \
				break;                                                        \
			case PS_TYPE:                                                     \
			case PS_END:                                                      \
				type.s = begin;                                               \
				type.len = c - begin;                                         \
				begin = 0;                                                    \
				if(cb(&key, &name, vl_get_param_type(&type), &type, userdata) \
						< 0)                                                  \
					return -1;                                                \
				break;                                                        \
			case PS_BEGIN:                                                    \
				goto err;                                                     \
		}                                                                     \
	}

#define CHECK_MATCH_S \
	if(match_s)       \
		goto err;     \
	else              \
		match_s = 1;

	while(*c) {
		switch(*c) {
			case ')':
				if(ps == PS_TYPE && include) {
					include--;
					break;
				}
				if((ps != PS_KEY && ps != PS_TYPE) || (ps == PS_KEY && !match_s)
						|| (ps == PS_TYPE && match_s))
					goto err;
				ps = PS_END;
				CHECK_AND_FILL_PARAMETER;
				break;
			case '(':
				if(ps == PS_TYPE) {
					include++;
					break;
				}
				if(ps != PS_KEY)
					goto err;
				CHECK_AND_FILL_PARAMETER;
				CHECK_MATCH_S;
				break;
			case ',':
				if(ps != PS_TYPE || include)
					goto err;
				CHECK_AND_FILL_PARAMETER;
				CHECK_MATCH_S;
				break;
			case ':':
				if(ps == PS_TYPE && include)
					break;
				if(ps != PS_NAME)
					goto err;
				CHECK_AND_FILL_PARAMETER;
				CHECK_MATCH_S;
				break;
			case ' ':
				CHECK_AND_FILL_PARAMETER;
				break;
			default:
				if(!begin && match_s) {
					if(ps == PS_BEGIN)
						ps = PS_KEY;
					else if(ps == PS_KEY)
						ps = PS_NAME;
					else if(ps == PS_NAME)
						ps = PS_TYPE;
					else if(ps == PS_TYPE)
						ps = PS_NAME;
					begin = c;
					match_s = 0;
				} else if(!begin && !match_s) {
					goto err;
				}
		}

		c++;
	}

	if(ps != PS_END)
		goto err;
	return 0;
err:
	LM_ERR("syntax error in mapping string \"%s\": col - %ld, symb - %c\n", val,
			(long)(c - val), *c);
	return -1;
#undef CHECK_AND_FILL_PARAMETER
#undef CHECK_MATCH_S
}
