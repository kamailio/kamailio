/*
 * Add "message-summary" event to presence module
 *
 * Copyright (C) 2007 Juha Heinanen
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 *  2007-05-1  initial version (jih)
 */

/*!
 * \file
 * \brief SIP-router Presence :: Message-waiting indication support
 * \ingroup presence
 * Module: \ref presence
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../parser/parse_content.h"
#include "../presence/event_list.h"
#include "presence_mwi.h"

/*! \brief utility function that skips spaces and tabs */
inline char *eat_sp_tab(char *at, char *over)
{
    while((at < over) && ((*at == ' ') || (*at == '\t'))) at++;
    return at;
}

/*! \brief utility function that skips printable ascii chars */
inline char *eat_printable(char *at, char *over)
{
    while ((at < over) && ((*at == '\t') || ((*at >= 32) && (*at <= 126))))
	at++;
    return at;
}

/*! \brief
 * event specific publish handling - check if body format is ok
 */
int mwi_publ_handl(struct sip_msg* msg)
{	
    str body;
    char *at, *over;

    if (get_content_length(msg) == 0)
	return 1;
	
    body.s = get_body(msg);
    if (body.s == NULL) {
	LM_ERR("cannot extract body from msg\n");
	return -1;
    }

    /* content-length (if present) must be already parsed */
    body.len = get_content_length(msg);
    at = body.s;
    over = body.s + body.len;

    /* check msg-status-line */
    if (body.len <= 16) goto err;
    if (strncmp(body.s, "Messages-Waiting", 16) != 0) goto err;
    at = at + 16;
    at = eat_sp_tab(at, over);
    if ((at >= over) || (*at != ':')) goto err;
    at++;
    if ((at >= over) || ((*at != ' ') && (*at != '\t'))) goto err;
    at++;
    at = eat_sp_tab(at, over);
    if (at + 3 >= over) goto err;
    if (strncmp(at, "yes", 3) == 0) at = at + 3;
    else
	if (strncmp(at, "no", 2) == 0) at = at + 2;
	else
	    goto err;
    if ((at + 1 >= over) || (*at != '\r') || (*(at + 1) != '\n')) goto err;
    at = at + 2;
    
    /* check that remaining body consists of lines that only contain
       printable ascii chars */
    while (at < over) {
	at = eat_printable(at, over);
	if ((at + 1 >= over) || (*at != '\r') || (*(at + 1) != '\n')) goto err;
	at = at + 2;
    }
    
    return 1;

err:
    LM_ERR("check of body <%.*s> failed at character number %d\n",
	   body.len, body.s, (int)(at - body.s + 1));
    return -1;

}

int mwi_add_events(void)
{
    pres_ev_t event;
	
    /* constructing message-summary event */
    memset(&event, 0, sizeof(pres_ev_t));
    event.name.s = "message-summary";
    event.name.len = 15;

    event.content_type.s = "application/simple-message-summary";
    event.content_type.len = 34;

	event.default_expires= 3600;
    event.type = PUBL_TYPE;
	event.req_auth = 0;
    event.evs_publ_handl = mwi_publ_handl;
	
    if (pres_add_event(&event) < 0) {
	LM_ERR("failed to add event \"message-summary\"\n");
	return -1;
    }		
	
    return 0;
}
