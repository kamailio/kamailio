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
 */
#ifndef SCA_SUBSCRIBE_H
#define SCA_SUBSCRIBE_H

#include "sca.h"
#include "sca_dialog.h"

enum {
    SCA_SUBSCRIPTION_STATE_ACTIVE,
    SCA_SUBSCRIPTION_STATE_PENDING,
    SCA_SUBSCRIPTION_STATE_TERMINATED,
    SCA_SUBSCRIPTION_STATE_TERMINATED_DEACTIVATED,
    SCA_SUBSCRIPTION_STATE_TERMINATED_GIVEUP,
    SCA_SUBSCRIPTION_STATE_TERMINATED_NORESOURCE,
    SCA_SUBSCRIPTION_STATE_TERMINATED_PROBATION,
    SCA_SUBSCRIPTION_STATE_TERMINATED_REJECTED,
    SCA_SUBSCRIPTION_STATE_TERMINATED_TIMEOUT,
};

struct _sca_subscription {
    str		subscriber;	/* contact: user@ip */
    str		target_aor;	/* account of record to watch: user@domain */
    int		event;		/* "call-info", "line-seize" */
    time_t	expires;	/* expiration date of subscription */
    int		state;		/* active, pending, terminated */
    int		index;		/* seized appearance-index, line-seize only */

    sca_dialog	dialog;		/* call-id, to- and from-tags, cseq */

    str		rr;		/* Record-Route header values */

    int		db_cmd_flag;	/* track whether to INSERT or UPDATE */
};
typedef struct _sca_subscription	sca_subscription;

enum {
    SCA_SUBSCRIPTION_TERMINATE_OPT_UNSUBSCRIBE = (1 << 0),
    SCA_SUBSCRIPTION_TERMINATE_OPT_RELEASE_APPEARANCE = (1 << 1),
};
#define SCA_SUBSCRIPTION_TERMINATE_OPT_DEFAULT \
	(SCA_SUBSCRIPTION_TERMINATE_OPT_UNSUBSCRIBE | \
	 SCA_SUBSCRIPTION_TERMINATE_OPT_RELEASE_APPEARANCE)

enum {
    SCA_SUBSCRIPTION_CREATE_OPT_DEFAULT = 0,
    SCA_SUBSCRIPTION_CREATE_OPT_RAW_EXPIRES = (1 << 0),
};

extern const str 	SCA_METHOD_SUBSCRIBE;

#define SCA_SUBSCRIPTION_IS_TERMINATED( sub1 ) \
	((sub1)->state >= SCA_SUBSCRIPTION_STATE_TERMINATED && \
		(sub1)->state <= SCA_SUBSCRIPTION_STATE_TERMINATED_TIMEOUT )

#define SCA_SUB_REPLY_ERROR( mod, scode, smsg, sreply ) \
        sca_subscription_reply((mod), (scode), (smsg), \
		SCA_EVENT_TYPE_CALL_INFO, -1, (sreply))

int	sca_handle_subscribe( sip_msg_t *, char *, char * );
int	sca_subscription_reply( sca_mod *, int, char *, int, int, sip_msg_t * );

int	sca_subscription_from_db_result( db1_res_t *, sca_subscription * );
int	sca_subscriptions_restore_from_db( sca_mod * );
int	sca_subscription_db_update( void );
void	sca_subscription_db_update_timer( unsigned, void * );
void	sca_subscription_purge_expired( unsigned int, void * );
void	sca_subscription_state_to_str( int, str * );

int	sca_subscription_aor_has_subscribers( int, str * );
int	sca_subscription_delete_subscriber_for_event( sca_mod *, str *, str *,
						      str * );
int	sca_subscription_terminate( sca_mod *, str *, int, str *, int, int );

#endif /* SCA_SUBSCRIBE_H */
