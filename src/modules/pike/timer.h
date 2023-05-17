/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 */


#ifndef _PIKE_TIMER_H
#define _PIKE_TIMER_H


typedef struct pike_list_link
{
	struct pike_list_link *next;
	struct pike_list_link *prev;
} pike_list_link_t;


#define has_timer_set(_ll) ((_ll)->prev || (_ll)->next)

#define is_list_empty(_head) ((_head)->next == (_head))

#define update_in_timer(_head, _ll)    \
	do {                               \
		remove_from_timer(_head, _ll); \
		append_to_timer(_head, _ll);   \
	} while(0)


void append_to_timer(pike_list_link_t *head, pike_list_link_t *ll);

void remove_from_timer(pike_list_link_t *head, pike_list_link_t *ll);

void check_and_split_timer(pike_list_link_t *head, unsigned int time,
		pike_list_link_t *split, unsigned char *mask);


#endif
