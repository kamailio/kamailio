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
 *
 * History:
 * ---------
 *  2004-02-06  created (bogdan)
 */


#include "sr_module.h"
#include "dprint.h"
#include "mem/mem.h"
#include "db/db.h"
#include "usr_avp.h"



static db_con_t  *avp_db_con = 0;
struct usr_avp   *users_avps = 0;
char             *avp_db_url = 0;


static char* usr_type[] = {"ruri","from","to",0};


int init_avp_child( int rank )
{
	if ( rank>PROC_MAIN ) {
		if (avp_db_con==0) {
			LOG(L_NOTICE,"NOTICE:init_avp_child: no avp_db_url specified "
				"-> feature disabled\n");
			return 0;
		}
		/* init db connection */
		if ( bind_dbmod(avp_db_url) < 0 ) {
			LOG(L_ERR,"ERROR:init_avp_child: unable to find any db module\n");
			return -1;
		}
		if ( (avp_db_con=db_init( avp_db_url ))==0) {
			/* connection failed */
			LOG(L_ERR,"ERROR:init_avp_child: unable to connect to database\n");
			return -1;
		}
	}

	return 0;
}



void destroy_avps( )
{
	struct usr_avp *avp;

	while (users_avps) {
		avp = users_avps;
		users_avps = users_avps->next;
		pkg_free( avp );
	}
}



int get_user_type( char *id )
{
	int i;

	for(i=0;usr_type[i];i++) {
		if (!strcasecmp( id, usr_type[i]) )
			return i;
	}

	LOG(L_ERR,"ERROR:avp:get_user_type: unknown user type <%s>\n",id);
	return -1;
}


int load_avp( struct sip_msg *msg, int type, char *attr, int use_dom)
{
	DBG("----load_avp:%d,%s,%d\n",type,attr?attr:"NULL",use_dom);
	return 1;
}

