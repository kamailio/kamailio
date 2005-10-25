/* 
 * Copyright (C) 2005 iptelorg GmbH
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

#ifndef __XCAP_CLIENT_H
#define __XCAP_CLIENT_H

typedef struct {
	/** full HTTP/HTTPS uri for the query */
	char *uri;
	/** username for authentication */
	char *auth_user;
	/** password used for authentication */
	char *auth_pass;
	/** Accept unverifiable peers (ignore information 
	 * stored in certificate and trust a certificate
	 * without know CA). */
	int enable_unverified_ssl_peer;
} xcap_query_t;

/** Sends a XCAP query to the destination and using parameters from 
 * query variable a returns received data in output variables buf
 * and bsize. */
int xcap_query(xcap_query_t *query, char **buf, int *bsize);

#endif
