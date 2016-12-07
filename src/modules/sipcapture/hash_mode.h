/*
 * Copyright (C) 2012 dragos.dinu@1and1.ro, 1&1 Internet AG
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
 */


#ifndef HASH_MODE_H
#define HASH_MODE_H 1




/*
 * Determines from which part of a message will be used to calculate the hash
 * Possible values are:
 * 
 * hs_call_id     the content of the Call-ID header field
 * hs_from_user   the username part of the URI in the From header field
 * hs_to_user     the username part of the URI in the To header field
 * hs_error       no hash specified
*/
enum hash_source {
	hs_call_id = 1,
	hs_from_user,
	hs_to_user,
	hs_error
};


/*
 * CRC32 hash function
 * Returns an integer number between 0 and denominator - 1 based on
 * the hash source from the msg. The hash algorithm is CRC32.
*/
int hash_func (struct _sipcapture_object * sco,
                         enum hash_source source, int denominator);

/*
 * Gets the hash source type.
*/
enum hash_source get_hash_source (const char *hash_source);

#endif
