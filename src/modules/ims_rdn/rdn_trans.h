/*
 * $Id$
 *
 * -----------------------------------------------------------------
 * Copyright Theodor Scherney
 * Copyright (C) 2016 - 2025 Kontron Transportation GmbH,
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
// This file added by %KTRVICT%
// Creating branch: devel_uui_mapping
// Modified by branches: n/a

#define DIGIT_2_SHORT(ch)                                                   \
	(short)(((ch) >= '0' && (ch) <= '9')                                    \
					? ((ch) - '0')                                          \
					: (((ch) >= 'A' && (ch) <= 'F')                         \
									  ? ((ch) - 'A' + 10)                   \
									  : (((ch) >= 'a' && (ch) <= 'f')       \
														? ((ch) - 'a' + 10) \
														: (-1))))

enum _tr_type
{
	TR_NONE = 0,
	TR_RDN
};

enum _tr_s_subtype
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

char *tr_parse_rdn(str *in, trans_t *tr);

/* Base16 encoding */

int base16_decode_update(char *dst, str *src);

int parse_uui(const str *uui_header, str *fn, int *pd);

int base16_encode_update(char *dst, size_t length, char *src);

int cast_int_to_hex_ts(char *dst, str *src);

int get_sds_disposition_type(size_t length, char *src);

int get_sds_message_type(size_t length, char *src);

int get_sds_notificatn_type(size_t length, char *src);

int get_sds_message_id(char *dst, size_t length, char *src);

int get_sds_date_time(char *dst, size_t length, char *src);

int put_sds_date_time(char *dst, size_t length, char *src);

int get_sds_signalling(char *dst, size_t length, char *src);

int extract_text_from_SDS(char **dst, str *src);

int build_sds_header_for_txt_body(char *dst, size_t length, char *src);
