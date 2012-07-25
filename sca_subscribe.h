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
};
typedef struct _sca_subscription	sca_subscription;

extern const str 	SCA_METHOD_SUBSCRIBE;

#define SCA_SUBSCRIPTION_IS_TERMINATED( sub1 ) \
	((sub1)->state >= SCA_SUBSCRIPTION_STATE_TERMINATED && \
		(sub1)->state <= SCA_SUBSCRIPTION_STATE_TERMINATED_TIMEOUT )

int	sca_handle_subscribe( sip_msg_t *, char *, char * );
int	sca_unsubscribe_line_seize( sip_msg_t *, char *, char * );

void	sca_subscription_purge_expired( unsigned int, void * );
void	sca_subscription_state_to_str( int, str * );

int	sca_subscription_terminate( sca_mod *, str *, int, str *, int );

#endif /* SCA_SUBSCRIBE_H */
