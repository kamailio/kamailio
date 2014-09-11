/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "pdb_server_backend.h"
#include "dtm.h"
#include "log.h"
#include <stdio.h>
#include <string.h>




struct dtm_node_t *mroot;




int init_backend(char *filename)
{
	mroot=dtm_load(filename);
	if (mroot == NULL) {
		LERR("cannot load '%s'.\n", filename);
		return -1;
	}
	return 0;
}




carrier_t lookup_number(char *number)
{
	carrier_t carrierid;
	int nmatch=dtm_longest_match(mroot, number, strlen(number), &carrierid);
	if (nmatch<=0) {
		/* nothing found - return id 0 */
		carrierid=0;
	}
	LINFO("request='%s', nmatch=%ld, carrier=%ld\n", number, (long int)nmatch, (long int)carrierid);
	return carrierid;
}
