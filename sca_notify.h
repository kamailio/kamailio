#ifndef SCA_NOTIFY_H
#define SCA_NOTIFY_H

#include "sca_subscribe.h"

extern const str	SCA_METHOD_NOTIFY;

int		sca_notify_subscriber( sca_mod *, sca_subscription *, int );
int		sca_notify_call_info_subscribers( sca_mod *, str * );

#endif /* SCA_NOTIFY_H */
