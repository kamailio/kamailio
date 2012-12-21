#ifndef __QSA_EVETNS_H
#define __QSA_EVETNS_H

#include "../../str.h"

int events_qsa_interface_init(int _handle_presence_subscriptions);
void events_qsa_interface_destroy();

/* default route for Events: presence */
extern str presence_route;
extern str presence_outbound_proxy;
extern str presence_headers;

#endif
