/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _UAC_CSEQ_H_
#define _UAC_CSEQ_H_

#include "../../core/parser/msg_parser.h"

int uac_cseq_update(sip_msg_t *msg);
int uac_cseq_refresh(sip_msg_t *msg);
int uac_cseq_register_callbacks(void);

#endif
