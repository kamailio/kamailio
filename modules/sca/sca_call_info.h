/*
 * Copyright (C) 2012 Andrew Mortensen
 *
 * This file is part of the sca module for Kamailio, a free SIP server.
 *
 * The sca module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The sca module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef SCA_CALL_INFO_H
#define SCA_CALL_INFO_H

#include "sca.h"
#include "sca_subscribe.h"

/* pass to sca_notify_subscriber to include all appearances in Call-Info hdr */
#define SCA_CALL_INFO_APPEARANCE_INDEX_ANY	0

enum {
    SCA_CALL_INFO_SHARED_NONE = 0,
    SCA_CALL_INFO_SHARED_CALLER = (1 << 0),
    SCA_CALL_INFO_SHARED_CALLEE = (1 << 1),
};
#define SCA_CALL_INFO_SHARED_BOTH \
	(SCA_CALL_INFO_SHARED_CALLER | SCA_CALL_INFO_SHARED_CALLEE)

struct _sca_call_info {
    str		sca_uri;
    int		index;
    int		state;
    str		uri;

    /* mask tracking which endpoints in a call are shared */
    int		ua_shared;
};
typedef struct _sca_call_info		sca_call_info;

#define SCA_CALL_INFO_EMPTY( ci1 ) \
	((void*)(ci1) == NULL || \
		((ci1)->index == SCA_CALL_INFO_APPEARANCE_INDEX_ANY && \
		(ci1)->state == SCA_APPEARANCE_STATE_UNKNOWN))

#define SCA_CALL_INFO_IS_SHARED_CALLER( ci1 ) \
	(!SCA_CALL_INFO_EMPTY((ci1)) && \
	(((sca_call_info *)(ci1))->ua_shared & SCA_CALL_INFO_SHARED_CALLER))

#define SCA_CALL_INFO_IS_SHARED_CALLEE( ci1 ) \
	(!SCA_CALL_INFO_EMPTY((ci1)) && \
	(((sca_call_info *)(ci1))->ua_shared & SCA_CALL_INFO_SHARED_CALLEE))


extern const str	SCA_CALL_INFO_HEADER_STR;


int sca_call_info_update( sip_msg_t *, char *, char * );
void sca_call_info_sl_reply_cb( void * );
void sca_call_info_ack_cb( struct cell *, int, struct tmcb_params * );

int sca_call_info_build_header( sca_mod *, sca_subscription *, char *, int );
int sca_call_info_append_header_for_appearance_index( sca_subscription *, int,
						      char *, int );

hdr_field_t *sca_call_info_header_find( hdr_field_t * );
int sca_call_info_body_parse( str *, sca_call_info * );
int sca_call_info_free( sca_call_info * );

#endif /* SCA_CALL_INFO_H */
