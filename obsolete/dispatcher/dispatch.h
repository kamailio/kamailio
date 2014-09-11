/**
 * $Id$
 *
 * dispatcher module
 * 
 * Copyright (C) 2004-2006 FhG Fokus
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

/**
 * History
 * -------
 * 2004-07-31  first version, by dcm
 * 
 */

#ifndef _DISPATCH_H_
#define _DISPATCH_H_

#include "../../parser/msg_parser.h"


#define DS_HASH_USER_ONLY 1  /* use only the uri user part for hashing */
#define DS_HASH_USER_OR_HOST 2  /* use user part of uri for hashing with
                                   fallback to host */
extern int ds_flags; 

int ds_set_hash_f(int n);

int ds_load_list(char *lfile);
int ds_destroy_list();
int ds_select_dst(struct sip_msg *msg, char *set, char *alg);
int ds_select_new(struct sip_msg *msg, char *set, char *alg);

#endif

