/*
 * Sdp mangler module
 *
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
/* History:
 * --------
 *  2003-04-07 first version.  
 */

#include "utils.h"

#include "../../parser/msg_parser.h"	/* struct sip_msg */
#include "../../mem/mem.h"
#include "../../data_lump.h"

#include <stdio.h>


int
patch (struct sip_msg *msg, char *oldstr, unsigned int oldlen, char *newstr,
       unsigned int newlen)
{
	unsigned int off;
	struct lump *anchor;

	if (oldstr == NULL)
		return -1;

	if (newstr == NULL)
		return -2;
	off = oldstr - msg->buf;
	if (off < 0)
		return -3;
	if ((anchor = del_lump (&msg->add_rm, off, oldlen, 0)) == 0)
	{
		LOG (L_ERR, "ERROR: error lumping with del_lump\n");
		return -4;
	}
	if ((insert_new_lump_after (anchor, newstr, newlen, 0)) == 0)
	{
		LOG (L_ERR,
		     "ERROR: error lumping with insert_new_lump_after\n");
		return -5;
	}

	return 0;
}


/* TESTED */
int
patch_content_length (struct sip_msg *msg, unsigned int newValue)
{

	struct hdr_field *contentLength;
	char *s, pos[11];
	int len;

	contentLength = msg->content_length;
	if (contentLength == NULL)	/* maybe not yet parsed */
	{
		if (parse_headers (msg, HDR_CONTENTLENGTH, 0) == -1)
		{
			LOG (L_ERR,
			     "ERROR: parse headers on Content-Length failed\n");
			return -1;
		}
		contentLength = msg->content_length;
		if (contentLength == NULL)
		{
			LOG (L_ERR,
			     "ERROR: parse headers on Content-Length succeded but msg->content_length is still NULL\n");
			return -2;
		}
	}
	/* perhaps dangerous because buffer is static ? */
	//pos = int2str(newValue,&len);
	len = snprintf ((char *) pos, 10, "%u", newValue);
	s = pkg_malloc (len);
	if (s == NULL)
	{
		LOG (L_ERR, "ERROR: unable to allocate %d bytes\n", len);
		return -3;
	}
	memcpy (s, pos, len);
	/* perhaps we made it and no one called int2str,migth use sprintf */
	if (patch
	    (msg, contentLength->body.s, contentLength->body.len, s, len) < 0)
	{
		pkg_free (s);
		LOG (L_ERR, "ERROR: lumping failed\n");
		return -4;
	}

	DBG ("DEBUG: Succeded in altering Content-Length to new value %u\n",
	     newValue);

	return 0;

}
