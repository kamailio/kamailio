/*
 * $Id$
 *
 * Copyright (C) 2005-2009 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 *  2009-01-19  first version (bogdan)
 */



#include <string.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../trim.h"
#include "prefix_tree.h"
#include "dr_bl.h"


static struct dr_bl *drbl_lists = NULL;

static char **bl_lists=NULL;
static unsigned int bl_lists_size = 0;


int set_dr_bl( modparam_t type, void* val)
{
	bl_lists = pkg_realloc( bl_lists, (bl_lists_size+1)*sizeof(char*));
	if (bl_lists==NULL) {
		LM_ERR("failed to realloc\n");
		return -1;
	}
	bl_lists[bl_lists_size] = (char*)val;
	bl_lists_size++;
	return 0;
}


int init_dr_bls(void)
{
	unsigned int i;
	struct dr_bl *drbl;
	str name;
	str val;
	char *p;

	if (bl_lists==NULL)
		return 0;

	for( i=0 ; i<bl_lists_size ; i++ ) {
		LM_DBG("processing bl definition <%s>\n",bl_lists[i]);
		/* get name */
		p = strchr( bl_lists[i], '=');
		if (p==NULL || p== bl_lists[i]) {
			LM_ERR("blaclist definition <%s> has no name",bl_lists[i]);
			return -1;
		}
		name.s = bl_lists[i];
		name.len = p - name.s;
		trim(&name);
		if (name.len==0) {
			LM_ERR("empty name in blacklist definition <%s>\n",bl_lists[i]);
			return -1;
		}
		LM_DBG("found list name <%.*s>\n",name.len,name.s);
		/* alloc structure */
		drbl = (struct dr_bl*)shm_malloc( sizeof(struct dr_bl) );
		if (drbl==NULL) {
			LM_ERR("no more shme memory\n");
			return -1;
		}
		memset( drbl, 0, sizeof(struct dr_bl));
		/* fill in the types */
		p++;
		do {
			if (drbl->no_types==MAX_TYPES_PER_BL) {
				LM_ERR("too many types per rule <%s>\n",bl_lists[i]);
				shm_free(drbl);
				return -1;
			}
			val.s = p;
			p = strchr( p, ',');
			if (p==NULL) {
				val.len = strlen(val.s);
			} else {
				val.len = (int)(long)(p - val.s);
				p++;
			}
			trim(&val);
			if (val.len==0) {
				LM_ERR("invalid types listing in <%s>\n",bl_lists[i]);
				shm_free(drbl);
				return -1;
			}
			LM_DBG("found type <%.*s>\n",val.len,val.s);
			if (str2int( &val, &drbl->types[drbl->no_types])!=0) {
				LM_ERR("nonnumerical type <%.*s>\n",val.len,val.s);
				shm_free(drbl);
				return -1;
			}
			drbl->no_types++;
		}while(p!=NULL);

		pkg_free(bl_lists[i]);
		bl_lists[i] = NULL;

		/* create backlist for it */
		//drbl->bl = create_bl_head( 131313, 0/*flags*/, NULL, NULL, &name);
		//if (drbl->bl==NULL) {
		//	LM_ERR("failed to create bl <%.*s>\n",name.len,name.s);
		//	shm_free(drbl);
		//	return -1;
		//}

		/* link it */
		drbl->next = drbl_lists;
		drbl_lists = drbl;
	}

	pkg_free(bl_lists);
	bl_lists = NULL;

	return 0;
}



void destroy_dr_bls(void)
{
	struct dr_bl *drbl;
	struct dr_bl *drbl1;

	for( drbl=drbl_lists ; drbl ; ) {
		drbl1 = drbl;
		drbl = drbl->next;
		shm_free(drbl1);
	}
}


int populate_dr_bls(pgw_addr_t *pgwa)
{
	unsigned int i;
	struct dr_bl *drbl;
	pgw_addr_t *gw;
	struct bl_rule *drbl_first;
	struct bl_rule *drbl_last;
	struct net *gw_net;

	/* each bl list at a time */
	for( drbl=drbl_lists ; drbl ; drbl = drbl->next ) {
		drbl_first = drbl_last = NULL;
		/* each type at a time */
		for ( i=0 ; i<drbl->no_types ; i++ ) {
			/* search in the GW list all GWs of this type */
			for( gw=pgwa ; gw ; gw=gw->next ) {
				if (gw->type==drbl->types[i]) {
					gw_net = mk_net_bitlen( &gw->ip, gw->ip.len*8);
					if (gw_net==NULL) {
						LM_ERR("failed to build net mask\n");
						continue;
					}
					/* add this destination to the BL */
					//add_rule_to_list( &drbl_first, &drbl_last,
					//	gw_net,
					//	NULL/*body*/,
					//	0/*port*/,
					//	PROTO_NONE/*proto*/,
					//	0/*flags*/);
					pkg_free(gw_net);
				}
			}
		}
		/* the new content for the BL */
		//if (add_list_to_head( drbl->bl, drbl_first, drbl_last, 1, 0)!=0) {
		//	LM_ERR("failed to update bl\n");
		//	return -1;
		//}
	}

	return 0;
}

