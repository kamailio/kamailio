#ifndef _SMS_FUNCS_H
#define  _SMS_FUNCS_H

#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../dprint.h"
#include "../../config.h"

	int sms_extract_body(struct sip_msg * , str *);

#endif
