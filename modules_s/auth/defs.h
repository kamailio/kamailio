/* 
 * $Id$ 
 *
 * Common definitions
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


#ifndef DEFS_H
#define DEFS_H

#define PARANOID

/*
 * Helper definitions
 */

/*
 * the module will accept and authorize also username
 * of form user@domain which some broken clients send
 */
#define USER_DOMAIN_HACK


/*
 * If the method is ACK, it is always authorized
 */
#define ACK_CANCEL_HACK


/* 
 * Send algorithm=MD5 in challenge
 */
#define PRINT_MD5

/*
 * If defined, realm parameter can be omitted and will
 * be extracted from SIP message
 */
#define REALM_HACK


#endif /* DEFS_H */
