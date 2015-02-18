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
#include "../../str.h"

#ifndef SCA_APPEARANCE_H
#define SCA_APPEARANCE_H

#include "sca_dialog.h"

enum {
    SCA_APPEARANCE_STATE_IDLE = 0,
    SCA_APPEARANCE_STATE_SEIZED,
    SCA_APPEARANCE_STATE_PROGRESSING,
    SCA_APPEARANCE_STATE_ALERTING,
    SCA_APPEARANCE_STATE_ACTIVE_PENDING,
    SCA_APPEARANCE_STATE_ACTIVE,
    SCA_APPEARANCE_STATE_HELD,
    SCA_APPEARANCE_STATE_HELD_PRIVATE,
    SCA_APPEARANCE_STATE_UNKNOWN = 0xff,
}; 
#define sca_appearance_is_held( app1 ) \
	((app1) && ((app1)->state == SCA_APPEARANCE_STATE_HELD || \
		   (app1)->state == SCA_APPEARANCE_STATE_HELD_PRIVATE))

enum {
    SCA_APPEARANCE_FLAG_DEFAULT = 0,
    SCA_APPEARANCE_FLAG_OWNER_PENDING = (1 << 0),
    SCA_APPEARANCE_FLAG_CALLEE_PENDING = (1 << 1),
};

enum {
    SCA_APPEARANCE_OK = 0,
    SCA_APPEARANCE_ERR_NOT_IN_USE = 0x1001,
    SCA_APPEARANCE_ERR_INDEX_INVALID = 0x1002,
    SCA_APPEARANCE_ERR_INDEX_UNAVAILABLE = 0x1004,
    SCA_APPEARANCE_ERR_MALLOC = 0x1008,
    SCA_APPEARANCE_ERR_UNKNOWN = 0x1f00,
};
#define SCA_APPEARANCE_INDEX_UNAVAILABLE	-2

/*
 * maximum lifetime of an active, pending appearance.
 * enough to allow retransmissions of the caller's
 * ACK. on receipt of the caller's ACK, we promote
 * the SCA callee's state to active.
 */
enum {
    /* Polycoms aggressively resubscribe line-seizes, give them time */
    SCA_APPEARANCE_STATE_SEIZED_TTL	= 120,

    /* enough time to allow retransmissions (~32s) */
    SCA_APPEARANCE_STATE_PENDING_TTL	= 35,
};

extern const str SCA_APPEARANCE_INDEX_STR;
extern const str SCA_APPEARANCE_STATE_STR;
extern const str SCA_APPEARANCE_URI_STR;

extern const str SCA_APPEARANCE_STATE_STR_IDLE;
extern const str SCA_APPEARANCE_STATE_STR_SEIZED;
extern const str SCA_APPEARANCE_STATE_STR_PROGRESSING;
extern const str SCA_APPEARANCE_STATE_STR_ALERTING;
extern const str SCA_APPEARANCE_STATE_STR_ACTIVE;
extern const str SCA_APPEARANCE_STATE_STR_HELD;
extern const str SCA_APPEARANCE_STATE_STR_HELD_PRIVATE;


struct _sca_appearance_times {
    /* time of appearance creation */
    time_t			ctime;

    /* time of last appearance state change */
    time_t			mtime;

    /* time of last end-to-end activity */
    time_t			atime;
};
typedef struct _sca_appearance_times	sca_appearance_times;

struct _sca_appearance_list;
struct _sca_appearance {
    int				index;
    int				state;
    str				uri;

    int				flags;

    str				owner;
    str				callee;
    sca_dialog			dialog;
    sca_appearance_times	times;

    str				prev_owner;
    str				prev_callee;
    sca_dialog			prev_dialog;

    struct _sca_appearance_list	*appearance_list;
    struct _sca_appearance	*next;
};
typedef struct _sca_appearance		sca_appearance;

struct _sca_appearance_list {
    str			aor;
    int			appearance_count;
    sca_appearance	*appearances;
};
typedef struct _sca_appearance_list	sca_appearance_list;

void	sca_appearance_state_to_str( int, str * );
int	sca_appearance_state_from_str( str * );

sca_appearance 	*sca_appearance_seize_index_unsafe( sca_mod *, str *, str *,
							int, int, int * );
int	sca_appearance_seize_index( sca_mod *, str *, int, str * );
int	sca_appearance_seize_next_available_index( sca_mod *, str *, str * );
sca_appearance 	*sca_appearance_seize_next_available_unsafe( sca_mod *, str *,
							     str *, int );
void	sca_appearance_update_state_unsafe( sca_appearance *, int );
int	sca_appearance_update_owner_unsafe( sca_appearance *, str * );
int	sca_appearance_update_callee_unsafe( sca_appearance *, str * );
int	sca_appearance_update_dialog_unsafe( sca_appearance *, str *,
						str *, str * );
int	sca_appearance_update_unsafe( sca_appearance *, int, str *, str *,
					sca_dialog *, str *, str * );
int	sca_appearance_update_index( sca_mod *, str *, int, int, str *,
					str *, sca_dialog * );
int	sca_appearance_release_index( sca_mod *, str *, int );
int	sca_appearance_owner_release_all( str *, str * );
int	sca_appearance_state_for_index( sca_mod *, str *, int );
sca_appearance	*sca_appearance_for_index_unsafe( sca_mod *, str *, int, int );
sca_appearance	*sca_appearance_for_dialog_unsafe( sca_mod *, str *,
						    sca_dialog *, int );
sca_appearance	*sca_appearance_for_tags_unsafe( sca_mod *, str *,
						str *, str *, str *, int );

int	sca_appearance_register( sca_mod *, str * );
int	sca_appearance_unregister( sca_mod *, str * );
void	sca_appearance_list_insert_appearance( sca_appearance_list *,
						sca_appearance * );
sca_appearance	*sca_appearance_list_unlink_index( sca_appearance_list *, int );
int		sca_appearance_list_unlink_appearance( sca_appearance_list *,
							sca_appearance ** );
sca_appearance	*sca_appearance_unlink_by_tags( sca_mod *, str *,
						str *, str *, str * );

sca_appearance	*sca_appearance_create( int, str * );
void		sca_appearance_free( sca_appearance * );

int		sca_uri_is_shared_appearance( sca_mod *, str * );
int		sca_uri_lock_shared_appearance( sca_mod *, str * );
int		sca_uri_lock_if_shared_appearance( sca_mod *, str *, int * );

void		sca_appearance_purge_stale( unsigned int, void * );
#endif /* SCA_APPEARANCE_H */
