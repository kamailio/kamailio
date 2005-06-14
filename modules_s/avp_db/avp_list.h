/*
 * $Id$
 *
 * Copyright (C) 2004 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "../../str.h"

/*
 * Structure holding AVP and its default value
 */
typedef struct avp_attr {
	str name;   /* name of attribute */
	str dval;   /* default value of attribute */
	int type;   /* type of attribute */
} avp_attr_t;

/*
 * Structure holding list of AVPs and their default values
 */
typedef struct avp_list {
	avp_attr_t*	avps;	
	int	n;						/* number of entries in avp_list */
} avp_list_t;


extern avp_list_t	**avp_list;		/* pointer to current list of AVPs*/

int init_avp_list();
int reload_avp_list();
void init_avp_use_def(int* a, avp_list_t *avp_l);
void reset_avp_use_def_flag(int* a, avp_list_t *avp_l, str* name);
void add_default_avps(int* a, avp_list_t *avp_l, str* prefix);
