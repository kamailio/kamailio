#ifndef _IM_FUNCS_H
#define  _IM_FUNCS_H

#include "../../str.h"
#include "../../parser/msg_parser.h"

	int im_extract_body(struct sip_msg * , str *);
	int im_get_user(struct sip_msg *, str* , str* );

#endif
