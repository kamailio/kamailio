#ifndef __CLIENT_RF_DIAMETER_RO_H
#define __CLIENT_RF_DIAMETER_RO_H

#include "../cdp/cdp_load.h"
#include "../cdp_avp/cdp_avp_mod.h"

int AAASendCCR(AAASession *session);

void RoChargingResponseHandler(AAAMessage *response, void *param);

#endif /* __CLIENT_RF_DIAMETER_RO_H */
