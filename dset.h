/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#ifndef _T_FORKS_H
#define _T_FORKS_H

#include "config.h"

#define CONTACT "Contact: "
#define CONTACT_LEN 9
#define CONTACT_DELIM ", "
#define CONTACT_DELIM_LEN 2


struct branch
{
	char uri[MAX_URI_SIZE];
	unsigned int len;
};

struct sip_msg;

/*
typedef int (*tfork_f)( struct sip_msg *msg, char *uri, int uri_len );
*/

/* add a new branch to current transaction */
int append_branch( struct sip_msg *msg, char *uri, int uri_len );
/* iterate through list of new transaction branches */
void init_branch_iterator();
char *next_branch( int *len );
void clear_branches();

char *print_dset( struct sip_msg *msg, int *len );
#endif
