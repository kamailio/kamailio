#include "../cdp/diameter_epc.h"
#include "Ro_data.h"
#include "diameter_ro.h"

int AAASendCCR(AAASession *session) {

    return 1;
}

/**
 * Handler for incoming Diameter requests.
 * @param request - the received request
 * @param param - generic pointer
 * @returns the answer to this request
 */
void RoChargingResponseHandler(AAAMessage *response, void *param) {
    switch (response->applicationId) {
        case IMS_Ro:
            switch (response->commandCode) {
                case Diameter_CCA:
                    break;
                default:
                    LM_ERR("ERR:"M_NAME":RoChargingResponseHandler: - "
                            "Received unknown response for Ro command %d, flags %#1x endtoend %u hopbyhop %u\n",
                            response->commandCode, response->flags,
                            response->endtoendId, response->hopbyhopId);
                    return;
            }
            break;
        default:
            LM_ERR("DBG:"M_NAME":RoChargingResponseHandler(): - Received unknown response for app %d command %d\n",
                    response->applicationId,
                    response->commandCode);
            LM_ERR("Reponse is [%s]", response->buf.s);
            return;

    }
    return;
}

