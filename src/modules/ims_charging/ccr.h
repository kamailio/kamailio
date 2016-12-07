#ifndef _Client_Ro_CCR_H
#define _Client_Ro_CCR_H

#include "../cdp/diameter.h"
#include "Ro_data.h"

AAAMessage *Ro_new_ccr(AAASession * session, Ro_CCR_t *);
Ro_CCA_t *Ro_parse_CCA_avps(AAAMessage *cca);

#endif
