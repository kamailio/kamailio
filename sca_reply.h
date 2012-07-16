#ifndef SCA_REPLY_H
#define SCA_REPLY_H

#include "sca_common.h"
#include "sca.h"

#define SCA_REPLY_ERROR( mod, scode, smsg, sreply ) \
	sca_reply((mod), (scode), (smsg), SCA_EVENT_TYPE_CALL_INFO, -1, \
		(sreply))

int	sca_reply( sca_mod *, int, char *, int, int, sip_msg_t * );

#endif /* SCA_REPLY_H */
