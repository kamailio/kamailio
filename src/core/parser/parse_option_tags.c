/*
 * Copyright (C) 2006 Andreas Granig <agranig@linguin.org>
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
 *
 */

#include "../mem/mem.h"
#include "parse_option_tags.h"

static inline void free_option_tag(struct option_tag_body **otb)
{
	if (otb && *otb) {
		pkg_free(*otb);
		*otb = 0;
	}
}

void hf_free_option_tag(void *parsed)
{
	struct option_tag_body *otb;
	otb = (struct option_tag_body *) parsed;
	free_option_tag(&otb);
}
