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


#ifndef UTILS_H
#define UTILS_H

#include "../../parser/msg_parser.h"

#define PARANOID


#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define SUCCESS 1
#define FAILURE 0

/*
 * Remove any leading white chars
 */
char* trim_leading(char* _s);

/*
 * Remove any trailing white chars
 */
char* trim_trailing(char* _s);

/*
 * Remove all leading and trailing white chars
 */
char* trim(char* _s);



/*
 * Substitute \r or \n with spaces
 */
struct hdr_field* remove_crlf(struct hdr_field* _hf);

/*
 * Convert string to lower case
 */
char* strlower(char* _s, int len);

/*
 * Convert string to upper case
 */
char* strupper(char* _s, int len);

/*
 * Find a character that is not quoted
 */
char* find_not_quoted(char* _b, char c);

/*
 * Skip the name part of a URL if any
 */
char* eat_name(char* _b);


/*
 * Converts binary array into its hex representation
 * Size of _hex must be _blen * 2
 */
void bin2hex(unsigned char* _hex, unsigned char* _bin, int _blen);

#endif
