/*
 * $Id$
 *
 * PIKE module
 *
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
/* History:
 * --------
 *  2004-05-12  created (bogdan)
 */




#include "../../dprint.h"
#include "../../fifo_server.h"
#include "../../locking.h"
#include "pike_fifo.h"
#include "ip_tree.h"


extern gen_lock_t*             timer_lock;
extern struct list_link*       timer;


int fifo_print_ip_tree( FILE *fifo_stream, char *response_file )
{
	FILE * rpl;

	/* open the repy fifo */
	rpl = open_reply_pipe( response_file );
	if (rpl==0) {
		LOG(L_ERR,"ERROR:pike:fifo_print_ip_tree: failed to open response "
			"file \"%s\"\n",response_file);
		goto error;
	}

	print_tree( rpl );

	fclose(rpl);
	return 0;
error:
	return -1;
}



int fifo_print_timer_list( FILE *fifo_stream, char *response_file )
{
	struct list_link *ll;
	FILE * rpl;

	/* open the repy fifo */
	rpl = open_reply_pipe( response_file );
	if (rpl==0) {
		LOG(L_ERR,"ERROR:pike:fifo_print_timer_list: failed to open "
			"response file \"%s\"\n",response_file);
		goto error;
	}

	/* lock and print the list */
	lock_get(timer_lock);
	for ( ll=timer->next ; ll!=timer; ll=ll->next) {
		fprintf(rpl," %p [byte=%d](expires=%d)\n",
			ll, ll2ipnode(ll)->byte, ll2ipnode(ll)->expires);
	}
	lock_release(timer_lock);


	fclose(rpl);
	return 0;
error:
	return -1;

}


