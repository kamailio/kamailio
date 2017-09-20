#ifndef SEC_AGREE_H
#define SEC_AGREE_H

#include "../ims_usrloc_pcscf/usrloc.h"

/**
 * Looks for the Security-Client header
 * @param msg - the sip message
 * @param params - ptr to struct sec_agree_params, where parsed values will be saved
 * @returns 0 on success, error code on failure
 */
int cscf_get_security(struct sip_msg *msg, security_t *params);

void free_security_t(security_t *params);

#endif // SEC_AGREE_H
