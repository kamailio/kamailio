
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
/*
 * tls initialization & cleanup functions
 * 
 * History:
 * --------
 *  2003-06-29  created by andrei
 */
#ifdef USE_TLS

#ifndef tls_init_h
#define tls_init_h

#ifndef USE_TCP
#error "TLS requires TCP support compiled-in, please" \
        " add -DUSE_TCP to the Makefile.defs"
#endif

#ifndef SHM_MEM
#error "shared memory support needed (add -DSHM_MEM to Makefile.defs)"
#endif


/* inits ser tls support
 * returns 0 on success, <0 on error */
int init_tls();

/* cleans up */
void destroy_tls();

/* inits a sock_info structure with tls data
 * (calls tcp_init for the tcp part)
 * returns 0 on success, -1 on error */
int tls_init(struct socket_info* sock_info);

/* print the ssl error stack */
void tls_dump_errors(char* s);


#endif /* tls_init_h*/
#endif /* USE_TLS*/
