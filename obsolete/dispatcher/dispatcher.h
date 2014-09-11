/**
 * $Id$
 *
 * dispatcher module
 * 
 * Copyright (C) 2004-2006 FhG Fokus
 * Copyright (C) 2005-2008 Hendrik Scholz <hendrik.scholz@freenet-ag.de>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _DISPATCHER_H_
#define _DISPATCHER_H_

#include "../../sr_module.h"

/* these are tunable
 *
 * if you increase no of sets or nodes to >99 output in di_rpc.c
 * might get garbled (increase %2d and %3d respectively)
 */
#define DS_MAX_SETS     32  /* maximum number of dispatcher sets */
#define DS_MAX_NODES    32  /* max number of nodes per set */
#define DS_MAX_URILEN   256 /* max SIP uri length */


/* no service needed below */
#define DS_HASH_USER_ONLY 1  /* use only the uri user part for hashing */
#define DS_HASH_USER_OR_HOST 2  /* use user part of uri for hashing with
                                   fallback to host */
/* MAX_LINE_LEN should be >= DS_MAX_URI_LEN to prevent overflow in
 * config file parser
 */
#define MAX_LINE_LEN    DS_MAX_URILEN

extern int ds_flags; 

/* prototypes */
int ds_load_list(char *lfile);
int ds_destroy_lists();
int ds_select_dst(struct sip_msg *msg, char *set, char *alg);
int ds_select_new(struct sip_msg *msg, char *set, char *alg);
int ds_init_memory(void);
void ds_clean_list(void);

#define DS_SWITCH_ACTIVE_LIST *ds_activelist = (0 == *ds_activelist) ? 1 : 0;

#endif /* _DISPATCHER_H_ */
