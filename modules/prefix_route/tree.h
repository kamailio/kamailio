/*
 * Prefix Route Module - tree search interface
 *
 * Copyright (C) 2007 Alfred E. Heggestad
 * Copyright (C) 2008 Telio Telecom AS
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
/*! \file
 * \brief Prefix Route Module - tree search interface
 * \ingroup prefix_route
 */


struct tree_item;

struct tree_item *tree_item_alloc(void);
void tree_item_free(struct tree_item *item);
int  tree_item_add(struct tree_item *root, const char *prefix,
		   const char *route, int route_ix);
int  tree_item_get(const struct tree_item *root, const str *user);
void tree_item_print(const struct tree_item *item, FILE *f, int level);


struct tree;

int  tree_init(void);
void tree_close(void);
int  tree_swap(struct tree_item *root);
int  tree_route_get(const str *user);
void tree_print(FILE *f);
