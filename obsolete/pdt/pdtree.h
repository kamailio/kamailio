/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2005 Voice Sistem SRL (Voice-System.RO)
 *
 * This file is part of SIP Express Router.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
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
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 * History:
 * -------
 * 2005-01-25  first tree version (ramona)
 */

		       
#ifndef _PDTREE_H_
#define _PDTREE_H_

#include "../../str.h"

typedef struct _pdt_node
{
	str domain;
	struct _pdt_node *child;
} pdt_node_t;

#define PDT_MAX_DEPTH	32

#define PDT_NODE_SIZE	10

typedef struct _pdt_tree
{
	pdt_node_t *head;
	int idsync;
} pdt_tree_t;


int pdt_add_to_tree(pdt_tree_t *pt, str *code, str *domain);
int pdt_remove_from_tree(pdt_tree_t *pt, str *code);
str* pdt_get_domain(pdt_tree_t *pt, str *code, int *plen);

pdt_tree_t* pdt_init_tree();
void pdt_free_tree(pdt_tree_t *pt);
int pdt_print_tree(pdt_tree_t *pt);

#endif

