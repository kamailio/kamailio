/*
 * Presence Agent, UNIX Domain Socket interface
 *
 * $Id$
 *
 * Copyright (C) 2003-2004 Hewlett-Packard Company
 * Copyright (C) 2004 FhG FOKUS
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

#include "../../unixsock_server.h"
#include "../../ut.h"
#include "dlist.h"
#include "pdomain.h"
#include "unixsock.h"


/*
 * Function for publishing events
 */
static int pa_publish(str* msg)
{
	unixsock_reply_asciiz("500 Not yet implemented\n");
	unixsock_reply_send();
	return -1;
}


/*
 * FIFO function for publishing presence
 *
 * :pa_presence:
 * pdomain (registrar or jabber)
 * presentity_uri
 * presentity_presence (civil or geopriv)
 *
 */
static int pa_presence(str* msg)
{
     // pdomain_t *pdomain = NULL;
     // presentity_t *presentity = NULL;
     str domain, uri, presence;
     // int origstate, newstate;
     // int allocated_presentity = 0;

     if (unixsock_read_line(&domain, msg) != 0) {
	  unixsock_reply_asciiz("400 Domain expected\n");
	  goto err;
     }

     if (unixsock_read_line(&uri, msg) != 0) {
	  unixsock_reply_asciiz("400 URI Expected\n");
	  goto err;
     }

     if (unixsock_read_line(&presence, msg) != 0) {
	  unixsock_reply_asciiz("400 Presence Expected\n");
	  goto err;
     }

#if 0
     domain.s[domain.len] = '\0'; /* We can safely zero-terminate here */
     register_pdomain(domain.s, &pdomain);
     if (!pdomain) {
	  unixsock_reply_asciiz("500 Could not register domain\n");
	  goto err;
     }

     find_presentity(pdomain, &uri, &presentity);
     if (!presentity) {
	  new_presentity(pdomain, &uri, &presentity);
	  add_presentity(pdomain, presentity);
	  allocated_presentity = 1;
     }
     if (!presentity) {
	  unixsock_reply_asciiz("500 Could not find presentity %.*s\n", uri.len, ZSW(uri.s));
	  goto err;
     }

     origstate = presentity->state;

     if ((presence.len == 6) && !memcmp(presence.s, "online", 6)) {
	  presentity->state = newstate = PS_ONLINE;
     } else {
	  presentity->state = newstate = PS_OFFLINE;
     }

     if (origstate != newstate || allocated_presentity) {
	  presentity->flags |= PFLAG_PRESENCE_CHANGED;
     }

     db_update_presentity(presentity);

#endif

     unixsock_reply_printf("200 Published\n(%.*s %.*s)\n", 
			   uri.len, ZSW(uri.s), presence.len, ZSW(presence.s));
     unixsock_reply_send();
     return 1;

 err:
     unixsock_reply_send();
     return -1;
}


/*
 * FIFO function for publishing location
 *
 * :pa_location:
 * pdomain (registrar or jabber)
 * presentity_uri
 * presentity_location (civil or geopriv)
 *
 */
static int pa_location(str* msg)
{
     // pdomain_t *pdomain = NULL;
     // presentity_t *presentity = NULL;
     str domain, uri, location;
     // int changed = 0;

     if (unixsock_read_line(&domain, msg) != 0) {
	  unixsock_reply_asciiz("400 Domain expected\n");
	  goto err;
     }

     if (unixsock_read_line(&uri, msg) != 0) {
	  unixsock_reply_asciiz("400 URI Expected\n");
	  goto err;
     }

     if (unixsock_read_line(&location, msg) != 0) {
	  unixsock_reply_asciiz("400 Location expected\n");
	  goto err;
     }

#if 0
     domain.s[domain.len] = '\0';
     register_pdomain(domain.s, &pdomain);
     if (!pdomain) {
	  unixsock_reply_asciiz("500 Could not register domain\n");
	  goto err;
     }

     find_presentity(pdomain, &uri, &presentity);
     if (!presentity) {
	  new_presentity(pdomain, &uri, &presentity);
	  add_presentity(pdomain, presentity);
	  changed = 1;
     }
     if (!presentity) {
	  unixsock_reply_asciiz("400 Could not find presentity\n");
	  goto err;
     }

     if ((presentity->location.loc.len == location.len) &&
	 memcmp(presentity->location.loc.s, location.s, location.len)) {
	  changed = 1;
     }

     memcpy(presentity->location.loc.s, location.s, location.len);
     presentity->location.loc.len = location.len;

     if (changed) {
	  presentity->flags |= PFLAG_PRESENCE_CHANGED;
     }

     db_update_presentity(presentity);

#endif

     unixsock_reply_printf("200 published\n",
			   "(%.*s %.*s)\n",
			   uri.len, ZSW(uri.s),
			   location.len, ZSW(location.s));
     unixsock_reply_send();
     return 1;

 err:
     unixsock_reply_send();
     return -1;
}


int init_unixsock_iface(void)
{
	if (unixsock_register_cmd("pa_publish", pa_publish) < 0) {
		return -1;
	}

	if (unixsock_register_cmd("pa_presence", pa_presence) < 0) {
		return -1;
	}

	if (unixsock_register_cmd("pa_location", pa_location) < 0) {
		return -1;
	}
	return 0;
}
