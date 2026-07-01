/*
 * ims_rdn module
 *
 * Copyright (C) 2023 Theodor Scherney
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../../core/sr_module.h"
#include "../../lib/ims/ims_getters.h"
#include "../../../src/core/mod_fix.h"
#include "../../../src/core/trim.h"
#include "../../../src/core/cfg/cfg_struct.h"
#include "rdn_trans.h"

/* parameters */

/*! transformation buffer size */
#define TR_BUFFER_SIZE 65536
#define TR_BUFFER_SLOTS 8

/*! transformation buffer */
static char **_tr_buffer_list = NULL;

static char *_tr_buffer = NULL;

static int _tr_buffer_idx = 0;

static tr_export_t mod_trans[] = {{{"rdn", sizeof("rdn") - 1}, /* rdn class */
										  tr_parse_rdn},
		{{0, 0}, 0}};

/*!
 *
 */
int tr_init_buffers(void)
{
	int i;

	_tr_buffer_list = (char **)malloc(TR_BUFFER_SLOTS * sizeof(char *));
	if(_tr_buffer_list == NULL)
		return -1;
	for(i = 0; i < TR_BUFFER_SLOTS; i++) {
		_tr_buffer_list[i] = (char *)malloc(TR_BUFFER_SIZE);
		if(_tr_buffer_list[i] == NULL)
			return -1;
	}
	return 0;
}

/*!
 *
 */
char *tr_set_crt_buffer(void)
{
	_tr_buffer = _tr_buffer_list[_tr_buffer_idx];
	_tr_buffer_idx = (_tr_buffer_idx + 1) % TR_BUFFER_SLOTS;
	return _tr_buffer;
}


/* -- transformations functions */

int base16_encode_update(char *dst, size_t length, char *src)
{
	size_t i;

	for(i = 0; i < (length - 1); i += 2) {
		dst[i] = src[i + 1];
		dst[i + 1] = src[i];
	}

	if(i < length) {
		dst[length] = src[length - 1];
		dst[length - 1] = 'F';
		length = length + 1;
	}
	return length;
}


int parse_uui(const str *uui_header, str *fn, int *pd)
{
	char *a;
	int n;
	int i = 0;
	char text[280];
	int length = 0;

	if(uui_header->len > 2) {
		if((uui_header->s[0] == '0') && (uui_header->s[1] == '0')) {
			a = strchr(uui_header->s, ';');
			if(a != NULL) {
				n = a - uui_header->s;
				strncpy(text, uui_header->s, n);
				text[n] = '\0';
				i = 2;
				while(i < (n - 3)) {
					length = (DIGIT_2_SHORT(text[i + 2])) * 16
							 + DIGIT_2_SHORT(text[i + 3]);
					if(text[i] == '0' && text[i + 1] == '5') {
						strncpy(fn->s, text + i + 4, (length * 2));
						fn->len = (length * 2);
						break;
					} else {
						i = i + 4 + (length * 2);
						length = 0;
					}
				}
				*pd = 0;
				return (length * 2);
			}
		}
		if((uui_header->s[0] == '0') && (uui_header->s[1] == '4')) {
			a = strchr(uui_header->s, ';');
			if(a != NULL) {
				n = a - uui_header->s - 2;
				fn->s = uui_header->s + 2;

				fn->len = n;
				*pd = 4;
				return n;
			}
		}
	}
	return 0;
}

int base16_decode_update(char *dst, str *src)
{
	char fn_text[64];
	str fn_str = {0, 0};
	fn_str.s = fn_text;
	fn_str.len = sizeof(fn_text);
	int length;
	int pd = 8;
	size_t i, j;

	length = parse_uui(src, &fn_str, &pd);

	if(length > 0) {
		if(pd == 0) {
			for(i = 0; i < (length - 1); i += 2) {
				dst[i] = fn_str.s[i + 1];
				dst[i + 1] = fn_str.s[i];
			}
			if(dst[i - 1] == 'F') {
				length = length - 1;
			}
			dst[length] = '\0';
			return length;
		} else {
			if(pd == 4) {
				j = 0;
				for(i = 0; i < (length - 1); i += 2) {
					dst[j] = fn_str.s[i + 1];
					j = j + 1;
				}
				dst[(j)] = '\0';
				return (length / 2);
			}
		}
	}
	return 0;
}

int cast_int_to_hex_ts(char *dst, str *src)
{
	src->s[src->len] = '\0';

	// Print timevalue as DEC number.
	// The string is ASCII encoded (not binary) because "append_message_body_hex()" transforms it into binary bytes anyway.
	int len = sprintf(dst, "%010lx", strtol(src->s, 0, 10));
	return len;
}

int get_sds_disposition_type(size_t length, char *src)
{
	int SDSindex = 0;
	char *points_to_SDS = NULL;

	// Find CR in string (what follows is: CR,LF,CR,LF)
	points_to_SDS = strchr(src, '\r');
	if(!points_to_SDS) {
		LM_ERR("Did not find character 0x0D\n");
		return 0;
	}
	// jump over CR,LF,CR,LF to find the Message-Type=0x01 of the SDS.
	SDSindex = points_to_SDS - src;
	SDSindex += 4;
	if(SDSindex >= length) {
		LM_ERR("Did not find Message Type in whole SIGNALLING PAYLOAD.\n");
		return 0;
	}
	// check Message Type = SDS SIGNALLING PAYLOAD
	if(src[SDSindex] != 0x01) {
		LM_ERR("wrong Message Type\n");
		return 0;
	}
	// jump to M parameter "Date and time"
	SDSindex++;
	// jump to M parameter "Conversation ID"
	SDSindex += 5;
	// jump to M parameter "Message ID"
	SDSindex += 16;
	// jump to first optional parameter
	SDSindex += 16;
	while(((0xf0 & src[SDSindex]) != 0x80) && (SDSindex < length)) {
		switch(src[SDSindex]) {
			case 21:
				// O parameter "InReplyTo message ID"
				SDSindex += 17;
				break;
			case 22:
				// O parameter "Application ID"
				SDSindex += 2;
				break;
			default:
				LM_ERR("Unknown optional parameter %02x\n",
						(unsigned char)src[SDSindex]);
				return 0;
		}
	}
	if((0xf0 & src[SDSindex]) != 0x80) {
		LM_ERR("Did not find optional parameter 0x80\n");
		return 0;
	}
	return (int)(0x0f & src[SDSindex]);
}

int get_sds_message_type(size_t length, char *src)
{
	int SDSindex = 0;
	char *points_to_SDS = NULL;

	// Find CR in string (what follows is: CR,LF,CR,LF)
	points_to_SDS = strchr(src, '\r');
	if(!points_to_SDS) {
		LM_ERR("Did not find character 0x0D\n");
		return 0;
	}
	// jump over CR,LF,CR,LF to find the Message-Type=0x01 of the SDS.
	SDSindex = points_to_SDS - src;
	SDSindex += 4;
	if(SDSindex >= length) {
		LM_ERR("Did not find Message Type in whole SIGNALLING PAYLOAD.\n");
		return 0;
	}
	return (unsigned char)src[SDSindex];
}

int get_sds_notificatn_type(size_t length, char *src)
{
	int SDSindex = 0;
	char *points_to_SDS = NULL;

	// Find CR in string (what follows is: CR,LF,CR,LF)
	points_to_SDS = strchr(src, '\r');
	if(!points_to_SDS) {
		LM_ERR("Did not find character 0x0D\n");
		return 0;
	}
	// jump over CR,LF,CR,LF to find the Message-Type=0x01 of the SDS.
	SDSindex = points_to_SDS - src;
	SDSindex += 4;
	if(SDSindex >= length) {
		LM_ERR("Did not find Message Type in whole SIGNALLING PAYLOAD.\n");
		return 0;
	}
	if(src[SDSindex] != 0x05) {
		LM_ERR("wrong Message Type\n");
		return 0;
	}
	// jump to M parameter "SDS disposition notification type"
	SDSindex++;
	return (unsigned char)src[SDSindex];
}

int get_sds_message_id(char *dst, size_t length, char *src)
{
	int SDSindex = 0;
	int i = 0;
	char *points_to_SDS = NULL;

	// Find CR in string (what follows is: CR,LF,CR,LF)
	points_to_SDS = strchr(src, '\r');
	if(!points_to_SDS) {
		LM_ERR("Did not find character 0x0D\n");
		return 0;
	}
	// jump over CR,LF,CR,LF to find the Message-Type=0x01 of the SDS.
	SDSindex = points_to_SDS - src;
	SDSindex += 4;
	if(SDSindex >= length) {
		LM_ERR("Did not find Message Type in whole SIGNALLING PAYLOAD.\n");
		return 0;
	}
	switch((unsigned char)src[SDSindex]) {
		case 0x01:
			// Message Type = SDS SIGNALLING PAYLOAD
			break;
		case 0x05:
			// Message Type = SDS NOTIFICATION
			// jump over SDS disposition notification type
			SDSindex++;
			break;
		default:
			LM_ERR("wrong Message Type\n");
			return 0;
	}

	// jump to M parameter "Date and time"
	SDSindex++;
	// jump to M parameter "Conversation ID"
	SDSindex += 5;
	// jump to M parameter "Message ID"
	SDSindex += 16;
	if(SDSindex + 16 >= length) {
		LM_ERR("Did not find Message ID in whole SIGNALLING PAYLOAD.\n");
		return 0;
	}
	// convert 16 binary octets into 32 ASCII characters
	for(i = 0; i < 16; i++) {
		sprintf(dst + (i << 1), "%02x", (unsigned char)src[SDSindex + i]);
	}
	return 32;
}

int get_sds_signalling(char *dst, size_t length, char *src)
{
	int SDSindex = 0;
	int i = 0;
	int len = 0;
	char *points_to_SDS = NULL;

	// Find ':' in string to calculate length of SDS signalling body
	points_to_SDS = strchr(src, ':');
	if(!points_to_SDS) {
		LM_ERR("Did not find character : (=colon)\n");
		return 0;
	}
	// Jump over spaces to get to the ASCI coded length
	i = points_to_SDS - src + 1;
	while(src[i] == ' ') {
		i++;
	}

	// Calculate length from ASCI coded decimal string
	while((src[i] >= '0') && (src[i] <= '9')) {
		len = (10 * len) + (src[i] - '0');
		i++;
	}

	// jump over CR,LF,CR,LF to find the Message-Type=0x01 of the SDS.
	SDSindex = i + 4;
	if(SDSindex >= length) {
		LM_ERR("Did not find Message Type in whole SIGNALLING PAYLOAD.\n");
		return 0;
	}
	// convert binary octets into ASCII characters
	for(i = 0; i < len; i++) {
		sprintf(dst + (i << 1), "%02x", (unsigned char)src[SDSindex + i]);
	}
	return (len << 1);
}

// No transformation back to binary string necessary (for Message ID),
// because "append_message_body_hex()" transforms ASCII into binary anyway
//  (2 ASCII bytes are transformed in 1 binary byte e.g. 0x31 0x61 into 0x1a).
//int put_sds_message_id(char *dst, size_t length, char *src)

int get_sds_date_time(char *dst, size_t length, char *src)
{
	int SDSindex = 0;
	int i = 0;
	unsigned long long int timevalue = 0;
	char *points_to_SDS = NULL;

	// Find CR in string (what follows is: CR,LF,CR,LF)
	points_to_SDS = strchr(src, '\r');
	if(!points_to_SDS) {
		LM_ERR("Did not find character 0x0D\n");
		return 0;
	}
	// jump over CR,LF,CR,LF to find the Message-Type=0x01 of the SDS.
	SDSindex = points_to_SDS - src;
	SDSindex += 4;
	if(SDSindex >= length) {
		LM_ERR("Did not find Message Type in whole SIGNALLING PAYLOAD.\n");
		return 0;
	}
	switch((unsigned char)src[SDSindex]) {
		case 0x01:
			// Message Type = SDS SIGNALLING PAYLOAD
			break;
		case 0x05:
			// Message Type = SDS NOTIFICATION
			// jump over SDS disposition notification type
			SDSindex++;
			break;
		default:
			LM_ERR("wrong Message Type\n");
			return 0;
	}

	// jump to M parameter "Date and time"
	SDSindex++;
	for(i = 0; i < 5; i++) {
		timevalue = (timevalue << 8);
		timevalue += (unsigned char)src[SDSindex + i];
	}
	return sprintf(dst, "%llu", timevalue);
}

int put_sds_date_time(char *dst, size_t length, char *src)
{
	int i = 0;
	long long int timevalue = 0;
	struct tm calender_time;

	// reads IMDN XML Body -> dateTime = .2024-03-13T14:21:58+0000.
	i = (src[0] - '0') * 10;
	i = (i + (src[1] - '0')) * 10;
	i = (i + (src[2] - '0')) * 10;
	i += (src[3] - '0');
	calender_time.tm_year = i - 1900;
	i = (src[5] - '0') * 10;
	i += (src[6] - '0');
	calender_time.tm_mon = i - 1;
	i = (src[8] - '0') * 10;
	i += (src[9] - '0');
	calender_time.tm_mday = i;
	i = (src[11] - '0') * 10;
	i += (src[12] - '0');
	calender_time.tm_hour = i;
	i = (src[14] - '0') * 10;
	i += (src[15] - '0');
	calender_time.tm_min = i;
	i = (src[17] - '0') * 10;
	i += (src[18] - '0');
	calender_time.tm_sec = i;
	i = (src[20] - '0') * 10;
	i += (src[21] - '0');
	if(src[19] == '+') {
		calender_time.tm_isdst = i;
	} else {
		calender_time.tm_isdst = -i;
	}
	timevalue = mktime(&calender_time);
	if(timevalue == -1) {
		LM_ERR("Could not transform calender time.\n");
		return 0;
	}
	// Print timevalue as HEX number.
	// The string is ASCII encoded (not binary) because "append_message_body_hex()" transforms it into binary bytes anyway.
	return sprintf(dst, "%010llx", timevalue);
}

int extract_text_from_SDS(char **dst, str *src)
{
	int len = 0;
	char *points_to_SDS = NULL;

	// set default value for output variable, in case there is an error.
	*dst = src->s;

	// Find CR in string (what follows is: CR,LF,CR,LF)
	points_to_SDS = strchr(src->s, '\r');
	if(!points_to_SDS) {
		LM_ERR("Did not find character 0x0D\n");
		return 0;
	}
	// jump over CR,LF,CR,LF to find the Message-Type=0x03 of the SDS.
	points_to_SDS += 4;
	if((unsigned char)points_to_SDS[0] != 3) {
		LM_ERR("wrong Message Type\n");
		return 0;
	}
	// jump to Number of Messages of the SDS.
	points_to_SDS++;
	if((unsigned char)points_to_SDS[0] != 1) {
		LM_ERR("SDS contains more than 1 message!\n");
		return 0;
	}
	// jump to first optional Parameter IEI in the SDS.
	points_to_SDS++;
	if((unsigned char)points_to_SDS[0] != 0x78) {
		// jump over first parameter
		if((unsigned char)points_to_SDS[0] == 0x7a) {
			// jump to 2 Byte Length of the parameter SDS.
			points_to_SDS++;
			len = (unsigned char)points_to_SDS[0];
			len = (len << 8);
			len += (unsigned char)points_to_SDS[1];
			// jump to next optional parameter.
			points_to_SDS = points_to_SDS + 2 + len;
			if((points_to_SDS[0])
					&& ((unsigned char)points_to_SDS[0] != 0x78)) {
				LM_ERR("unknown second parameter in SDS\n");
				return 0;
			}
		} else {
			LM_ERR("unknown first parameter in SDS\n");
			return 0;
		}
	}
	// jump to 2 Byte Length of the parameter SDS.
	points_to_SDS++;
	len = (unsigned char)points_to_SDS[0];
	len = (len << 8);
	len += (unsigned char)points_to_SDS[1];
	// jump to SDS content (format identifier).
	points_to_SDS += 2;
	if((unsigned char)points_to_SDS[0] != 1) {
		LM_ERR("SDS is not in text format\n");
		return 0;
	}
	// jump to SDS content (text).
	points_to_SDS++;

	// set output parameter dst.
	*dst = points_to_SDS;
	// return length of text only (without format identifier).
	return len - 1;
}

int build_sds_header_for_txt_body(char *dst, size_t length, char *src)
{
	int len = length;

	// add Message Type = 03
	dst[0] = 0x03;
	// add Number of Messages
	dst[1] = 0x01;
	// add IEI of optional Parameter
	dst[2] = 0x78;
	// add "Type of Format" to Parameter Length
	len++;
	//dst[3] = len/256;
	dst[3] = (len >> 8);
	//dst[4] = len%256;
	dst[4] = (len & 0x00ff);
	// add "Type of Format" = txt
	dst[5] = 0x01;
	// return length of SDS header
	return 6;
}

/*!
 * \brief Evaluate string transformations
 * \param msg SIP message
 * \param tp transformation
 * \param subtype transformation type
 * \param val pseudo-variable
 * \return 0 on success, -1 on error
 */
int tr_eval_rdn(
		struct sip_msg *msg, tr_param_t *tp, int subtype, pv_value_t *val)
{
	int length;
	char tr_buffer_local[20];
	char *points_to_SDS_text = NULL;

	memset(tr_buffer_local, 0, sizeof(tr_buffer_local));

	if(val == NULL || val->flags & PV_VAL_NULL) {
		LM_ERR("Error\n");
		return -1;
	}

	tr_set_crt_buffer();

	switch(subtype) {
		case TR_RDN_UUI2FN:
			length = base16_decode_update(_tr_buffer, &val->rs);
			_tr_buffer[length] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			LM_INFO("string: [%.*s]\n", length, _tr_buffer);
			val->rs.s = _tr_buffer;
			val->rs.len = length;
			break;
		case TR_RDN_FN2UUI:
			length = base16_encode_update(_tr_buffer, val->rs.len, val->rs.s);
			_tr_buffer[length] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			LM_INFO("string: [%.*s])\n", length, _tr_buffer);
			val->rs.s = _tr_buffer;
			val->rs.len = length;
			break;
		case TR_RDN_INT2HEXTS:
			length = cast_int_to_hex_ts(_tr_buffer, &val->rs);
			_tr_buffer[length] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			LM_INFO("string: [%.*s])\n", length, _tr_buffer);
			val->rs.s = _tr_buffer;
			val->rs.len = length;
			break;
		case TR_RDN_BIN2MSG_TYPE:
			// read first character (message type) from SDS SIGNALLING-PAYLOAD
			// or from SDS NOTIFICATION
			// and cast it as integer.
			val->ri = get_sds_message_type(val->rs.len, val->rs.s);
			// change type of pvar into integer
			val->flags = PV_TYPE_INT | PV_VAL_INT | PV_VAL_STR;
			LM_INFO("integer: [%i])\n", val->ri);
			// set string value of PVar for printing
			val->rs.s = int2str(val->ri, &val->rs.len);
			break;
		case TR_RDN_BIN2NOTIFICATN_TYPE:
			// read SDS disposition notification type
			// or from SDS NOTIFICATION
			// and cast it as integer.
			val->ri = get_sds_notificatn_type(val->rs.len, val->rs.s);
			// change type of pvar into integer
			val->flags = PV_TYPE_INT | PV_VAL_INT | PV_VAL_STR;
			LM_INFO("integer: [%i])\n", val->ri);
			// set string value of PVar for printing
			val->rs.s = int2str(val->ri, &val->rs.len);
			break;
		case TR_RDN_BIN2INT:
			// read any character from binary message body
			// and cast it as integer.
			val->ri = (unsigned char)val->rs.s[0];
			// change type of pvar into integer
			val->flags = PV_TYPE_INT | PV_VAL_INT | PV_VAL_STR;
			LM_INFO("integer: [%i])\n", val->ri);
			// set string value of PVar for printing
			val->rs.s = int2str(val->ri, &val->rs.len);
			break;
		case TR_RDN_BIN2DISPOSITION:
			// read "SDS disposition request type" from SDS SIGNALLING-PAYLOAD
			val->ri = get_sds_disposition_type(val->rs.len, val->rs.s);
			// Check for Error
			if(!val->ri)
				return -1;
			// change type of pvar into integer
			val->flags = PV_TYPE_INT | PV_VAL_INT | PV_VAL_STR;
			LM_INFO("integer: [%i])\n", val->ri);
			// set string value of PVar for printing
			val->rs.s = int2str(val->ri, &val->rs.len);
			break;
		case TR_RDN_BIN2MSG_ID:
			// read "MESSAGE ID" from SDS SIGNALLING-PAYLOAD
			length = get_sds_message_id(_tr_buffer, val->rs.len, val->rs.s);
			// Check for Error
			if(!length)
				return -1;
			_tr_buffer[length] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			LM_INFO("string: [%.*s])\n", length, _tr_buffer);
			val->rs.s = _tr_buffer;
			val->rs.len = length;
			break;
		case TR_RDN_BIN2DATE_TIME:
			// read "Date and Time value" from SDS SIGNALLING-PAYLOAD
			length = get_sds_date_time(_tr_buffer, val->rs.len, val->rs.s);
			// Check for Error
			if(!length)
				return -1;
			_tr_buffer[length] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			LM_INFO("string: [%.*s])\n", length, _tr_buffer);
			val->rs.s = _tr_buffer;
			val->rs.len = length;
			break;
		case TR_RDN_DATE_TIME2BIN:
			// compute and write "Date and Time value" for SDS SIGNALLING-PAYLOAD
			length = put_sds_date_time(_tr_buffer, val->rs.len, val->rs.s);
			// Check for Error
			if(!length)
				return -1;
			_tr_buffer[length] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			LM_INFO("string: [%.*s])\n", length, _tr_buffer);
			val->rs.s = _tr_buffer;
			val->rs.len = length;
			break;
		case TR_RDN_MSG_BODY_2_TXT:
			// get pointer to txt message from SDS DATA-PAYLOAD
			length = extract_text_from_SDS(&points_to_SDS_text, &val->rs);

			// Check for Error
			if(!length)
				return -1;

			// prepare the result of the transformation.
			val->rs.s = points_to_SDS_text;
			val->rs.len = length;
			val->flags = PV_VAL_STR;
			val->ri = 0;
			LM_INFO("string: [%.*s])\n", val->rs.len, val->rs.s);
			break;
		case TR_RDN_MSG_SIGN_2_TXT:
			// read SDS SIGNALLING-PAYLOAD
			length = get_sds_signalling(_tr_buffer, val->rs.len, val->rs.s);
			// Check for Error
			if(!length)
				return -1;
			_tr_buffer[length] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			LM_INFO("string: [%.*s])\n", length, _tr_buffer);
			val->rs.s = _tr_buffer;
			val->rs.len = length;
			break;
		case TR_RDN_TXT_2_MSG_BODY:
			// build SDS header for SDS DATA-PAYLOAD
			length = build_sds_header_for_txt_body(
					_tr_buffer, val->rs.len, val->rs.s);
			_tr_buffer[length] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			LM_INFO("string: [%.*s])\n", length, _tr_buffer);
			val->rs.s = _tr_buffer;
			val->rs.len = length;
			break;
		default:
			LM_ERR("unknown subtype %d (cfg line: %d)\n", subtype,
					get_cfg_crt_line());
			return -1;
	}
	return 0;
}


char *tr_parse_rdn(str *in, trans_t *t)
{
	char *p;
	str name;
	pv_spec_t *spec = NULL;
	tr_param_t *tp = NULL;

	LM_INFO("tr_parse_rdn \n");
	if(in == NULL || t == NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_RDN;
	t->trf = tr_eval_rdn;

	/* find next token */
	while(is_in_str(p, in) && *p != TR_PARAM_MARKER && *p != TR_RBRACKET)
		p++;
	if(*p == '\0') {
		LM_ERR("invalid transformation: %.*s\n", in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len == 6 && strncasecmp(name.s, "uui2fn", 6) == 0) {
		t->subtype = TR_RDN_UUI2FN;
		goto done;
	} else if(name.len == 6 && strncasecmp(name.s, "fn2uui", 6) == 0) {
		t->subtype = TR_RDN_FN2UUI;
		goto done;
	} else if(name.len == 9 && strncasecmp(name.s, "int2hexts", 9) == 0) {
		t->subtype = TR_RDN_INT2HEXTS;
		goto done;
	} else if(name.len == 7 && strncasecmp(name.s, "bin2int", 7) == 0) {
		t->subtype = TR_RDN_BIN2INT;
		goto done;
	} else if(name.len == 8 && strncasecmp(name.s, "bin2disp", 8) == 0) {
		t->subtype = TR_RDN_BIN2DISPOSITION;
		goto done;
	} else if(name.len == 9 && strncasecmp(name.s, "bin2msgid", 9) == 0) {
		t->subtype = TR_RDN_BIN2MSG_ID;
		goto done;
	} else if(name.len == 10 && strncasecmp(name.s, "bin2msgtyp", 10) == 0) {
		t->subtype = TR_RDN_BIN2MSG_TYPE;
		goto done;
	} else if(name.len == 10 && strncasecmp(name.s, "bin2nottyp", 10) == 0) {
		t->subtype = TR_RDN_BIN2NOTIFICATN_TYPE;
		goto done;
	} else if(name.len == 10 && strncasecmp(name.s, "bin2dattim", 10) == 0) {
		t->subtype = TR_RDN_BIN2DATE_TIME;
		goto done;
	} else if(name.len == 10 && strncasecmp(name.s, "dattim2bin", 10) == 0) {
		t->subtype = TR_RDN_DATE_TIME2BIN;
		goto done;
	} else if(name.len == 10 && strncasecmp(name.s, "msgbdy2txt", 10) == 0) {
		t->subtype = TR_RDN_MSG_BODY_2_TXT;
		goto done;
	} else if(name.len == 11 && strncasecmp(name.s, "msgsign2txt", 11) == 0) {
		t->subtype = TR_RDN_MSG_SIGN_2_TXT;
		goto done;
	} else if(name.len == 10 && strncasecmp(name.s, "txt2msgbdy", 10) == 0) {
		t->subtype = TR_RDN_TXT_2_MSG_BODY;
		goto done;
	}

	LM_ERR("unknown transformation: %.*s/%.*s/%d!\n", in->len, in->s, name.len,
			name.s, name.len);
error:
	if(tp)
		tr_param_free(tp);
	if(spec)
		pv_spec_free(spec);
	return NULL;
done:
	t->name = name;
	return p;
}


int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(tr_init_buffers() < 0) {
		LM_ERR("failed to initialize transformations buffers\n");
		return -1;
	}
	LM_INFO("mod_register_success\n");
	return register_trans_mod(path, mod_trans);
}
