#include "../../str.h"

#ifndef SCA_APPEARANCE_H
#define SCA_APPEARANCE_H

#include "sca_dialog.h"

enum {
    SCA_APPEARANCE_STATE_IDLE = 0,
    SCA_APPEARANCE_STATE_SEIZED,
    SCA_APPEARANCE_STATE_PROGRESSING,
    SCA_APPEARANCE_STATE_ALERTING,
    SCA_APPEARANCE_STATE_ACTIVE,
    SCA_APPEARANCE_STATE_HELD,
    SCA_APPEARANCE_STATE_HELD_PRIVATE,
    SCA_APPEARANCE_STATE_UNKNOWN = 0xff,
}; 

enum {
    SCA_APPEARANCE_FLAG_DEFAULT = 0,
    SCA_APPEARANCE_FLAG_OWNER_PENDING = (1 << 0),
    SCA_APPEARANCE_FLAG_CALLEE_PENDING = (1 << 1),
};

enum {
    SCA_APPEARANCE_OK = 0,
    SCA_APPEARANCE_ERR_NOT_IN_USE = 0x1001,
    SCA_APPEARANCE_ERR_INVALID_INDEX = 0x1002,
    SCA_APPEARANCE_ERR_MALLOC = 0x1004,
    SCA_APPEARANCE_ERR_UNKNOWN = 0x1f00,
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


struct _sca_appearance_list;
struct _sca_appearance {
    int				index;
    int				state;
    str				uri;

    int				flags;

    str				owner;
    str				callee;
    sca_dialog			dialog;

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
    int			max_index;
    int			next_index;
};
typedef struct _sca_appearance_list	sca_appearance_list;

void	sca_appearance_state_to_str( int, str * );
int	sca_appearance_state_from_str( str * );

int	sca_appearance_seize_next_available_index( sca_mod *, str *, str * );
sca_appearance 	*sca_appearance_seize_next_available_unsafe( sca_mod *, str *,
							     str *, int );
int	sca_appearance_update_owner_unsafe( sca_appearance *, str * );
int	sca_appearance_update_callee_unsafe( sca_appearance *, str * );
int	sca_appearance_update_dialog_unsafe( sca_appearance *, str *,
						str *, str * );
int	sca_appearance_update_unsafe( sca_appearance *, int, str *,
					sca_dialog *, str *, str * );
int	sca_appearance_update_index( sca_mod *, str *, int, int, str *,
					sca_dialog * );
int	sca_appearance_release_index( sca_mod *, str *, int );
int	sca_appearance_state_for_index( sca_mod *, str *, int );
sca_appearance	*sca_appearance_for_index_unsafe( sca_mod *, str *, int, int );
sca_appearance	*sca_appearance_for_dialog_unsafe( sca_mod *, str *,
						    sca_dialog *, int );
sca_appearance	*sca_appearance_for_tags_unsafe( sca_mod *, str *,
						str *, str *, str *, int );

int	sca_appearance_register( sca_mod *, str * );
void	sca_appearance_list_insert_appearance( sca_appearance_list *,
						sca_appearance * );
sca_appearance	*sca_appearance_list_unlink_index( sca_appearance_list *, int );
sca_appearance	*sca_appearance_unlink_by_tags( sca_mod *, str *,
						str *, str *, str * );

sca_appearance	*sca_appearance_create( int, str * );
void		sca_appearance_free( sca_appearance * );

int		sca_uri_is_shared_appearance( sca_mod *, str * );
int		sca_uri_lock_shared_appearance( sca_mod *, str * );
int		sca_uri_lock_if_shared_appearance( sca_mod *, str *, int * );
#endif /* SCA_APPEARANCE_H */
