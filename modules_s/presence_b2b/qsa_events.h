#ifndef __QSA_EVETNS_H
#define __QSA_EVETNS_H

#include "../../str.h"

int events_qsa_interface_init();
void events_qsa_interface_destroy();

/* default route for Events: presence */
extern str presence_route;

#endif
