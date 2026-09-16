/*
 * ims_rdn module -  rdn (railway dedicated network) related functions
 * for an IMS environment (S-CSCF and/or I-CSCF).
 *
 * -----------------------------------------------------------------
 * Copyright Theodor Scherney
 * Copyright (C) 2016 - 2026 Kontron Transportation GmbH,
 *               theodor.scherney@kontron.com
 *               christoph.eckl@kontron.com
 *               luca.nardin@kontron.com
 *               christoph.valentin@kontron.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _RDN_TRANS_H_
#define _RDN_TRANS_H_

#define RDN_DIGIT_2_SHORT(ch)                                               \
	(short)(((ch) >= '0' && (ch) <= '9')                                    \
					? ((ch) - '0')                                          \
					: (((ch) >= 'A' && (ch) <= 'F')                         \
									  ? ((ch) - 'A' + 10)                   \
									  : (((ch) >= 'a' && (ch) <= 'f')       \
														? ((ch) - 'a' + 10) \
														: (-1))))

enum _rdn_tr_type
{
	RDN_TR_NONE = 0,
	TR_RDN
};

enum _rdn_tr_s_subtype
{
	TR_RDN_NONE = 0,
	TR_RDN_UUI2FN,
	TR_RDN_FN2UUI,
	TR_RDN_INT2HEXTS,
	TR_RDN_BIN2INT,
	TR_RDN_BIN2DISPOSITION,
	TR_RDN_BIN2MSG_TYPE,
	TR_RDN_BIN2NOTIFICATN_TYPE,
	TR_RDN_BIN2MSG_ID,
	TR_RDN_BIN2DATE_TIME,
	TR_RDN_DATE_TIME2BIN,
	TR_RDN_MSG_BODY_2_TXT,
	TR_RDN_MSG_SIGN_2_TXT,
	TR_RDN_TXT_2_MSG_BODY
};

void rdn_tr_destroy_buffers(void);

#endif