/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#ifndef BIN_UTILS_H
#define	BIN_UTILS_H

#include "usrloc.h"

typedef struct _bin_data {
	char* s; /*string*/
	int len; /*string len*/
	int max; /*allocated size of the buffer s*/ 
} bin_data;

/*
 *		Binary encoding functions
 */
/* memory allocation and initialization macros */
#define BIN_ALLOC_METHOD    shm_malloc
#define BIN_REALLOC_METHOD  shm_realloc
#define BIN_FREE_METHOD     shm_free

inline int bin_alloc(bin_data *x, int max_len);
inline int bin_realloc(bin_data *x, int delta);
inline int bin_expand(bin_data *x, int delta);
inline void bin_free(bin_data *x);
inline void bin_print(bin_data *x);

inline int bin_encode_char(bin_data *x,char k);
inline int bin_decode_char(bin_data *x,char *c);

inline int bin_encode_uchar(bin_data *x,unsigned char k); 
inline int bin_decode_uchar(bin_data *x,unsigned char *c);

inline int bin_encode_short(bin_data *x,short k);
inline int bin_decode_short(bin_data *x,short *c);

inline int bin_encode_ushort(bin_data *x,unsigned short k); 
inline int bin_decode_ushort(bin_data *x,unsigned short *c);

inline int bin_encode_int(bin_data *x,int k);
inline int bin_decode_int(bin_data *x,int *c);

inline int bin_encode_uint(bin_data *x,unsigned int k); 
inline int bin_decode_uint(bin_data *x,unsigned int *c);

inline int bin_encode_time_t(bin_data *x,time_t k);
inline int bin_decode_time_t(bin_data *x,time_t *c);

inline int bin_encode_str(bin_data *x,str *s);
inline int bin_decode_str(bin_data *x,str *s);

int bin_encode_ims_subscription(bin_data *x, ims_subscription *s);
ims_subscription *bin_decode_ims_subscription(bin_data *x);

#endif	/* BIN_UTILS_H */

