/*
 * Copyright (C) 2015 Carsten Bock, ng-voice GmbH
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
 *
 */

/*! \file
 * \brief Support for transformations
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/ut.h"
#include "../../core/trim.h"
#include "../../core/pvapi.h"
#include "../../core/dset.h"
#include "../../core/basex.h"

#include "../../core/strutils.h"
#include "../../core/parser/parse_content.h"


#include "smsops_impl.h"

// Types of RP-DATA
typedef enum _rp_message_type {
	RP_DATA_MS_TO_NETWORK = 0x00,
	RP_DATA_NETWORK_TO_MS = 0x01,
	RP_ACK_MS_TO_NETWORK  = 0x02,
	RP_ACK_NETWORK_TO_MS  = 0x03,
} rp_message_type_t;

enum SMS_DATA {
	SMS_ALL,
	SMS_RPDATA_TYPE,
	SMS_RPDATA_REFERENCE,
	SMS_RPDATA_ORIGINATOR,
	SMS_RPDATA_DESTINATION,
	SMS_TPDU_TYPE,
	SMS_TPDU_FLAGS,
	SMS_TPDU_CODING,
	SMS_TPDU_PAYLOAD,
	SMS_TPDU_PROTOCOL,
	SMS_TPDU_VALIDITY,
	SMS_TPDU_REFERENCE,
	SMS_TPDU_ORIGINATING_ADDRESS,
	SMS_TPDU_DESTINATION,
	SMS_UDH_CONCATSM_REF,
	SMS_UDH_CONCATSM_MAX_NUM_SM,
	SMS_UDH_CONCATSM_SEQ,
	SMS_TPDU_ORIGINATING_ADDRESS_FLAGS,
	SMS_TPDU_DESTINATION_FLAGS,
	SMS_RPDATA_ORIGINATOR_FLAGS,
	SMS_RPDATA_DESTINATION_FLAGS
};

// Types of the PDU-Message
typedef enum _pdu_message_type {
	DELIVER = 0x00,
	SUBMIT = 0x01,
	COMMAND = 0x02,
	ANY = 0x03,
} pdu_message_type_t;

#define TP_RD 0x4;
#define TP_VPF 0x16;
#define TP_SRR 0x32;
#define TP_UDHI 0x64;
#define TP_RP 0x128;

// Information element identifiers and corresponding structs.
// Only the supported ones are listed.
// Defined in TS 23.040, Sec. 9.2.3.24 and 9.2.3.24.1
#define TP_UDH_IE_CONCAT_SM_8BIT_REF 0x00	// 9.2.3.24
struct ie_concat_sm_8bit_ref {
	unsigned char ref;			//Concatenated short message reference number
	unsigned char max_num_sm;	//Maximum number of short messages in the concatenated short message.
	unsigned char seq;			//Sequence number of the current short message
};

// Information element in User Data Header
typedef struct _tp_udh_inf_element tp_udh_inf_element_t;
struct _tp_udh_inf_element {
	unsigned char identifier;
	union {
		str data;
		struct ie_concat_sm_8bit_ref concat_sm_8bit_ref;
	};

	tp_udh_inf_element_t* next;
};

// TS 23.040, Sec. 9.2.3.24
typedef struct _tp_user_data {
	tp_udh_inf_element_t* header;
	str sm;
} tp_user_data_t;

// PDU (GSM 03.40) of the SMS
typedef struct _sms_pdu {
	pdu_message_type_t msg_type;
	unsigned char reference;
	unsigned char flags;
	unsigned char pid;
	unsigned char coding;
	unsigned char validity;
	str originating_address;
	str destination;
	tp_user_data_t payload;
	unsigned char originating_address_flags;
	unsigned char destination_flags;
	struct tm time; // Either SCTS (DELIVER) or VP (SUBMIT)
} sms_pdu_t;

// RP-Data of the message
typedef struct _sms_rp_data {
	rp_message_type_t msg_type;
	unsigned char reference;
	str originator;
	str destination;
	unsigned char pdu_len;
	sms_pdu_t pdu;
	unsigned char originator_flags;
	unsigned char destination_flags;
} sms_rp_data_t;

// Pointer to current parsed rp_data/tpdu
static sms_rp_data_t * rp_data = NULL;
// Pointer to rp_data/tpdu used for sending
static sms_rp_data_t * rp_send_data = NULL;

// ID of current message
static unsigned int current_msg_id = 0;

/*******************************************************************
 * Helper Functions
 *******************************************************************/

// Frees internal pointers within the SMS-PDU-Type:
void freeRP_DATA(sms_rp_data_t * rpdata) {
	if (rpdata) {
		if (rpdata->originator.s) pkg_free(rpdata->originator.s);
		if (rpdata->destination.s) pkg_free(rpdata->destination.s);
		if (rpdata->pdu.originating_address.s) pkg_free(rpdata->pdu.originating_address.s);
		if (rpdata->pdu.destination.s) pkg_free(rpdata->pdu.destination.s);
		while (rpdata->pdu.payload.header) {
			tp_udh_inf_element_t* next = rpdata->pdu.payload.header->next;
			if(rpdata->pdu.payload.header->identifier != TP_UDH_IE_CONCAT_SM_8BIT_REF) {
				if(rpdata->pdu.payload.header->data.s) pkg_free(rpdata->pdu.payload.header->data.s);
			}
			pkg_free(rpdata->pdu.payload.header);
			rpdata->pdu.payload.header = next;
		}
		if (rpdata->pdu.payload.sm.s) pkg_free(rpdata->pdu.payload.sm.s);
	}
}

#define BITMASK_7BITS 0x7F
#define BITMASK_8BITS 0xFF
#define BITMASK_HIGH_4BITS 0xF0
#define BITMASK_LOW_4BITS 0x0F
#define BITMASK_TP_UDHI 0x40
#define BITMASK_TP_VPF 0x18
#define BITMASK_TP_VPF_RELATIVE 0x10 
#define BITMASK_TP_VPF_ENHANCED 0x08
#define BITMASK_TP_VPF_ABSOLUTE 0x18

#define GSM7BIT_ESCAPE	0x1B

static unsigned char gsm7bit_codes[128] = {
0x40,0xA3,0x24,0xA5,0x65,0x65,0xF9,0xEC,
0x6F,0x43,0x0A,0x4F,0x6F,0x0D,0x41,0x61,
0xFF,0x5F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0x1B,0xC6,0xE6,0xDF,0x45,
0x20,0x21,0x22,0x23,0xA4,0x25,0x26,0x27,
0x28,0X29,0x2A,0x2B,0x2C,0xAD,0x2E,0x2F,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
0xA1,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
0x58,0x59,0x5A,0x41,0x4F,0x4E,0x55,0xA7,
0xBF,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
0x78,0x79,0x7A,0x61,0x6F,0x6E,0x75,0x61
};

static unsigned char gsm7bit_ext_codes[128] = {
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0x0C,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0x5E,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x7B,0x7D,0xFF,0xFF,0xFF,0xFF,0xFF,0x5C,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0x5B,0x7E,0x5D,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x7C,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0x45,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

static unsigned char ascii2gsm7bit_codes[256] = {
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 07
0xFF,0xFF,0x0A,0xFF,0x1B,0x0D,0xFF,0xFF, // 15
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 23
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 31
0x20,0x21,0x22,0x23,0x02,0x25,0x26,0x27, // 39
0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F, // 47
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37, // 55
0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F, // 63
0x00,0x41,0x42,0x43,0x44,0x45,0x46,0x47, // 71
0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F, // 79
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57, // 87
0x58,0x59,0x5A,0x1B,0x1B,0x1B,0x1B,0x11, // 95
0xFF,0x61,0x62,0x63,0x64,0x65,0x66,0x67, // 103
0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F, // 111
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77, // 119
0x78,0x79,0x7A,0x1B,0x1B,0x1B,0x1B,0xFF, // 127
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 135
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 143
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 151
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 159
0xFF,0x40,0xFF,0x01,0x23,0x03,0xFF,0x5F, // 167
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 175
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 183
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x60, // 191
0xFF,0xFF,0xFF,0xFF,0x5B,0x0E,0x1C,0x09, // 199
0xFF,0x1F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 207
0xFF,0x5D,0xFF,0xFF,0xFF,0xFF,0x5C,0xFF, // 215
0x0B,0xFF,0xFF,0xFF,0x5E,0xFF,0xFF,0x1E, // 223
0x7F,0xFF,0xFF,0xFF,0x7B,0x0F,0x1D,0xFF, // 231
0x04,0x05,0xFF,0xFF,0x07,0xFF,0xFF,0xFF, // 239
0xFF,0x7D,0x08,0xFF,0xFF,0xFF,0x7C,0xFF, // 247
0x0C,0x06,0xFF,0xFF,0x7E,0xFF,0xFF,0xFF, // 255
};

static unsigned char ascii2gsm7bit_ext_codes[256] = {
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 07
0xFF,0xFF,0xFF,0xFF,0x0A,0xFF,0xFF,0xFF, // 15
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 23
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 31
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 39
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 47
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 55
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 63
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 71
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 79
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 87
0xFF,0xFF,0xFF,0x3C,0x2F,0x3E,0x14,0xFF, // 95
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 103
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 111
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 119
0xFF,0xFF,0xFF,0x28,0x40,0x29,0x3D,0xFF, // 127
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 135
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 143
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 151
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 159
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 167
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 175
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 183
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 191
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 199
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 207
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 215
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 223
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 231
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 239
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 247
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, // 255
};

// Encode SMS-Message by merging 7 bit ASCII characters into 8 bit octets.
static int ascii_to_gsm(str sms, char * output_buffer, int buffer_size, int* output_text_size, unsigned char paddingBits) {
	// Check if output buffer is big enough.
	if ((sms.len * 7 + 7) / 8 > buffer_size)
		return 0;

	int output_buffer_length = 0;
	int carry_on_bits = 0;
	int i = 0;
	unsigned char symbol;
	int out_size = 0;
	char* tmp_buff = (char*)pkg_malloc(sms.len * 2);

	if(tmp_buff == NULL) {
		LM_ERR("Error allocating memory to encode sms text\n");
		return -1;
	}

	memset(tmp_buff, 0, sms.len * 2);

	for (; i < sms.len; ++i) {
		if(ascii2gsm7bit_codes[(unsigned char)sms.s[i]] == GSM7BIT_ESCAPE) {
			tmp_buff[out_size++] = GSM7BIT_ESCAPE;
			tmp_buff[out_size++] = ascii2gsm7bit_ext_codes[(unsigned char)sms.s[i]];
		} else {
			tmp_buff[out_size++] = ascii2gsm7bit_codes[(unsigned char)sms.s[i]];
		}
	}

	*output_text_size = out_size;

	if (paddingBits) {
		carry_on_bits = 7 - paddingBits;
 		output_buffer[output_buffer_length++] = tmp_buff[0] << (7 - carry_on_bits);
 		carry_on_bits++;
	}

	for (i = 0; i < out_size; ++i) {
		if (carry_on_bits == 7) {
			carry_on_bits = 0;
			continue;
		}

		symbol = (tmp_buff[i] & BITMASK_7BITS) >> carry_on_bits;

		if (i < out_size - 1) {
			symbol |= tmp_buff[i + 1] << (7 - carry_on_bits);
		}

		output_buffer[output_buffer_length++] = symbol;
		carry_on_bits++;
	}

	pkg_free(tmp_buff);

	return output_buffer_length;
}

// Decode 7bit encoded message by splitting 8 bit encoded buffer into 7 bit ASCII characters.
int gsm_to_ascii(char* buffer, int buffer_length, str sms, const int fill_bits) {
		int output_text_length = 0;

		if(!buffer_length || (fill_bits && buffer_length < 2))
			return 0;

		// How many bits we have carried from the next octet. This number can be positive or negative:
		// positive: We have carried n bits FROM the next octet.
		// negative: We have to carry n bits TO the next octet.
		// 0: Nothing carried. Default value!
		int carry_on_bits = 0;

		// Used to iterate over buffer. Declared here, because if there are fill_bits it have to be incremented
		int i = 0, num_bytes = 0;

		unsigned char symbol;
		unsigned char isEscape = 0; 

		// First remove the fill bits, if any
		if(fill_bits) {
			// We need 7 bits in the first octet, so if there is only 1 fill bit, we don't have to
			// carry from the next octet.

			// cmask stands for carry mask or how many bits to carry from the 2nd octet
			unsigned char cmask = (1 << (fill_bits - 1)) - 1;

			symbol = ( (buffer[0] >> fill_bits) |	// remove the fill bits from the first octet
											(buffer[1] & cmask << (8 - fill_bits)) // mask the required number of bits
																					//and shift them accordingly
										) & BITMASK_7BITS;	// mask just 7 bits from the first octet

			if (symbol != GSM7BIT_ESCAPE) {
				sms.s[output_text_length++] = gsm7bit_codes[symbol];
			}else{
				isEscape = 1;
			}

			carry_on_bits = fill_bits - 1;
			i++;
			num_bytes++;
		}


		for (; i < buffer_length; ++i) {
			if(carry_on_bits > 0) {
				unsigned char cmask = (1 << (carry_on_bits - 1)) - 1;	//mask for the rightmost carry_on_bits
																		//E.g. carry_on_bits=3 -> _ _ _ _ _ X X X
				symbol = ( (buffer[i] >> carry_on_bits) | //shift right to remove carried bits
												(buffer[i+1] & cmask) << (8 - carry_on_bits)	// carry from the next
																								// and shift accordingly
												) & BITMASK_7BITS;	// mask just 7 bits from the first octet
			}
			else if(carry_on_bits < 0) {
				carry_on_bits = carry_on_bits * -1;	//make carry_on_bits positive for the bitwise ops
				unsigned char cmask = ((1 << carry_on_bits) - 1) << (8 - carry_on_bits);	//mask for the leftmost carry_on_bits.
																						//E.g. carry_on_bits=3 -> X X X _ _ _ _ _
				symbol = ( (buffer[i] << carry_on_bits) | //shift left to make space for the carried bits
												(buffer[i-1] & cmask) >> (8 - carry_on_bits)	// get the bits from the previous octet
																								// and shift accordingly
												) & BITMASK_7BITS;	// mask just 7 bits from the first octet

				carry_on_bits = carry_on_bits * -1;	//return the original value
			}
			else {// carry_on_bits == 0
				symbol = buffer[i] & BITMASK_7BITS;
			}

			if (isEscape) {
				isEscape = 0;

				sms.s[output_text_length++] = gsm7bit_ext_codes[symbol];
			} else {
				if (symbol != GSM7BIT_ESCAPE) {
					sms.s[output_text_length++] = gsm7bit_codes[symbol];
				} else {
					isEscape = 1;
				}
			}

			//Update carry_on bits. It is always decremented, because we iterate over octests but read just septets
			carry_on_bits--;

			if (++num_bytes == sms.len) break;

			if (carry_on_bits == -8) {
				carry_on_bits = -1;
				symbol = buffer[i] & BITMASK_7BITS;

				if (isEscape) {
					isEscape = 0;

					sms.s[output_text_length++] = gsm7bit_ext_codes[symbol];
				} else {
					if (symbol != GSM7BIT_ESCAPE) {
						sms.s[output_text_length++] = gsm7bit_codes[symbol];
					} else {
						isEscape = 1;
					}
				}

				if (++num_bytes == sms.len) break;
			}

			if(carry_on_bits > 0 && (i + 2 >= buffer_length)) {
				//carry_on_bits is positive, which means thah we have to borrow from the next octet on next iteration
				//However i + 2 >= buffer_length so there is no next octet. This is error.
				break;
			}
		}

		if (num_bytes < sms.len) { // Add last remainder.
			symbol = buffer[i - 1] >> (8 - carry_on_bits);

			sms.s[output_text_length++] = gsm7bit_codes[symbol];
		}

		return output_text_length;
}

// Decode UCS2 message by splitting the buffer into utf8 characters
int ucs2_to_utf8 (int ucs2, char * utf8) {
	if (ucs2 < 0x80) {
		utf8[0] = ucs2;
		utf8[1] = 0;
		return 1;
	}
	if (ucs2 >= 0x80  && ucs2 < 0x800) {
		utf8[0] = (ucs2 >> 6)   | 0xC0;
		utf8[1] = (ucs2 & 0x3F) | 0x80;
		return 2;
	}
	if (ucs2 >= 0x800 && ucs2 < 0xFFFF) {
		if (ucs2 >= 0xD800 && ucs2 <= 0xDFFF) return -1;
		utf8[0] = ((ucs2 >> 12)       ) | 0xE0;
		utf8[1] = ((ucs2 >> 6 ) & 0x3F) | 0x80;
		utf8[2] = ((ucs2      ) & 0x3F) | 0x80;
		return 3;
	}
	if (ucs2 >= 0x10000 && ucs2 < 0x10FFFF) {
	utf8[0] = 0xF0 | (ucs2 >> 18);
	utf8[1] = 0x80 | ((ucs2 >> 12) & 0x3F);
	utf8[2] = 0x80 | ((ucs2 >> 6) & 0x3F);
	utf8[3] = 0x80 | ((ucs2 & 0x3F));
		return 4;
	}
	return -1;
}

// Decode UTF8 to UCS2
int utf8_to_ucs2 (const unsigned char * input, const unsigned char ** end_ptr) {
	*end_ptr = input;
	if (input[0] == 0)
		return -1;
	if (input[0] < 0x80) {
		* end_ptr = input + 1;
		return input[0];
	}
	if ((input[0] & 0xE0) == 0xE0) {
		if (input[1] == 0 || input[2] == 0) return -1;
		*end_ptr = input + 3;
		return (input[0] & 0x0F) << 12 | (input[1] & 0x3F) << 6  | (input[2] & 0x3F);
	}
	if ((input[0] & 0xC0) == 0xC0) {
		if (input[1] == 0) return -1;
		* end_ptr = input + 2;
		return (input[0] & 0x1F) << 6 | (input[1] & 0x3F);
	}
	return -1;
}

// Encode a digit based phone number for SMS based format.
static int EncodePhoneNumber(str phone, char * output_buffer, int buffer_size) {
	int output_buffer_length = 0;
	// Check if the output buffer is big enough.
	if ((phone.len + 1) / 2 > buffer_size)
		return -1;

	int i = 0;
	for (; i < phone.len; ++i) {
		if (phone.s[i] < '0' && phone.s[i] > '9')
			return -1;
		if (i % 2 == 0) {
			output_buffer[output_buffer_length++] =	BITMASK_HIGH_4BITS | (phone.s[i] - '0');
		} else {
			output_buffer[output_buffer_length - 1] = (output_buffer[output_buffer_length - 1] & BITMASK_LOW_4BITS) | ((phone.s[i] - '0') << 4);
		}
	}

	return output_buffer_length;
}

// Decode a digit based phone number for SMS based format.
static int DecodePhoneNumber(char* buffer, int len, str phone) {
	int i = 0;
	for (; i < len; ++i) {
		if (i % 2 == 0)
			phone.s[i] = (buffer[i / 2] & BITMASK_LOW_4BITS) + '0';
		else
			phone.s[i] = ((buffer[i / 2] & BITMASK_HIGH_4BITS) >> 4) + '0';
	}
	return i;
}

// Generate a 7 Byte Long Time
static void EncodeTime(char * buffer) {
	time_t ts;
	struct tm now;
	int i = 0;

	time(&ts);
	/* Get GMT time */
	gmtime_r(&ts, &now);

	i = now.tm_year % 100;
	buffer[0] = (unsigned char)((((i % 10) << 4) | (i / 10)) & 0xff);
	i = now.tm_mon + 1;
	buffer[1] = (unsigned char)((((i % 10) << 4) | (i / 10)) & 0xff);
	i = now.tm_mday;
	buffer[2] = (unsigned char)((((i % 10) << 4) | (i / 10)) & 0xff);
	i = now.tm_hour;
	buffer[3] = (unsigned char)((((i % 10) << 4) | (i / 10)) & 0xff);
	i = now.tm_min;
	buffer[4] = (unsigned char)((((i % 10) << 4) | (i / 10)) & 0xff);
	i = now.tm_sec;
	buffer[5] = (unsigned char)((((i % 10) << 4) | (i / 10)) & 0xff);
	buffer[6] = 0; // Timezone, we use no time offset.
}

static void DecodeTime(char* buffer, struct tm* decoded_time)
{
	time_t ts;

	time(&ts);
	/* Get GMT time */
	gmtime_r(&ts, decoded_time);

	decoded_time->tm_year	= 100 + ((buffer[0] & 0x0F) * 10) + ((buffer[0] >> 4) & 0x0F);
	decoded_time->tm_mon	= ((buffer[1] & 0x0F) * 10) + ((buffer[1] >> 4) & 0x0F) - 1;
	decoded_time->tm_mday	= ((buffer[2] & 0x0F) * 10) + ((buffer[2] >> 4) & 0x0F);
	decoded_time->tm_hour	= ((buffer[3] & 0x0F) * 10) + ((buffer[3] >> 4) & 0x0F);
	decoded_time->tm_min	= ((buffer[4] & 0x0F) * 10) + ((buffer[4] >> 4) & 0x0F);
	decoded_time->tm_sec	= ((buffer[5] & 0x0F) * 10) + ((buffer[5] >> 4) & 0x0F);
}

//The function is called GetXXX but it actually creates the IE if it doesn't exist
static struct ie_concat_sm_8bit_ref* GetConcatShortMsg8bitRefIE(sms_rp_data_t* rp_data)
{
	tp_udh_inf_element_t* ie = rp_data->pdu.payload.header;
	tp_udh_inf_element_t* prev = rp_data->pdu.payload.header;
	//Look for Concatenated SM 8bit Reference IE
	while(ie) {
		if(ie->identifier == TP_UDH_IE_CONCAT_SM_8BIT_REF)
			break;
		prev = ie;
		ie = ie->next;
	}

	if(ie == NULL) {
		//If not found - create it
		ie = pkg_malloc(sizeof(tp_udh_inf_element_t));
		if(ie == NULL) {
			LM_ERR("no more pkg\n");
			return NULL;
		}
		memset(ie, 0, sizeof(tp_udh_inf_element_t));
		ie->identifier = TP_UDH_IE_CONCAT_SM_8BIT_REF;

		if(prev) {
			//If the previous IE is not NULL - link to it
			prev->next = ie;
		}
		else {
			//There are not IEs at all
			rp_data->pdu.payload.header = ie;
			//Set TP-UDHI flag to 1
			rp_data->pdu.flags |= BITMASK_TP_UDHI;
		}
	}

	return &(ie->concat_sm_8bit_ref);
}

// Decode SMS-Body into the given structure:
int decode_3gpp_sms(struct sip_msg *msg) {
	str body;
	int len, blen, j, p = 0;
	// Parse only the body again, if the message differs from the last call:
	if (msg->id != current_msg_id) {
		// Extract Message-body and length: taken from RTPEngine's code
		body.s = get_body(msg);
		if (body.s == 0) {
			LM_ERR("failed to get the message body\n");
			return -1;
		}

		/*
		 * Better use the content-len value - no need of any explicit
		 * parsing as get_body() parsed all headers and Content-Length
		 * body header is automatically parsed when found.
		 */
		if (msg->content_length==0) {
			LM_ERR("failed to get the content length in message\n");
			return -1;
		}

		body.len = get_content_length(msg);
		if (body.len==0) {
			LM_ERR("message body has length zero\n");
			return -1;
		}

		if (body.len + body.s > msg->buf + msg->len) {
			LM_ERR("content-length exceeds packet-length by %d\n",
					(int)((body.len + body.s) - (msg->buf + msg->len)));
			return -1;
		}

		// Get structure for RP-DATA:
		if (!rp_data) {
			rp_data = (sms_rp_data_t*)pkg_malloc(sizeof(struct _sms_rp_data));
			if (!rp_data) {
				LM_ERR("Error allocating %lu bytes!\n", (unsigned long)sizeof(struct _sms_rp_data));
				return -1;
			}
		} else {
			freeRP_DATA(rp_data);
		}

		// Initialize structure:
		memset(rp_data, 0, sizeof(struct _sms_rp_data));

		////////////////////////////////////////////////
		// RP-Data
		////////////////////////////////////////////////
		rp_data->msg_type = (unsigned char)body.s[p++];
		rp_data->reference = (unsigned char)body.s[p++];
		if ((rp_data->msg_type == RP_DATA_MS_TO_NETWORK) || (rp_data->msg_type == RP_DATA_NETWORK_TO_MS)) {
			len = body.s[p++];
			if (len > 0) {
				p++; // Type of Number, we assume E164, thus ignored
				len--; // Deduct the type of number from the len
				rp_data->originator.s = pkg_malloc(len * 2);
				rp_data->originator.len = DecodePhoneNumber(&body.s[p], len * 2, rp_data->originator);
				p += len;
			}
			len = body.s[p++];
			if (len > 0) {
				p++; // Type of Number, we assume E164, thus ignored
				len--; // Deduct the type of number from the len
				rp_data->destination.s = pkg_malloc(len * 2);
				rp_data->destination.len = DecodePhoneNumber(&body.s[p], len * 2, rp_data->destination);
				p += len;
			}

			////////////////////////////////////////////////
			// TPDU
			////////////////////////////////////////////////
			rp_data->pdu_len = body.s[p++];
			if (rp_data->pdu_len > 0) {
				rp_data->pdu.flags = (unsigned char)body.s[p++];
				rp_data->pdu.msg_type = (unsigned char)rp_data->pdu.flags & 0x03;

				if (rp_data->pdu.msg_type == SUBMIT || rp_data->pdu.msg_type == COMMAND) {
					rp_data->pdu.reference = (unsigned char)body.s[p++];
				}

				if (rp_data->pdu.msg_type == SUBMIT) {
					// TP-DA
					rp_data->pdu.destination.len = body.s[p++];
					if (rp_data->pdu.destination.len > 0) {
						rp_data->pdu.destination_flags = (unsigned char)body.s[p++];
						rp_data->pdu.destination.s = pkg_malloc(rp_data->pdu.destination.len);
						DecodePhoneNumber(&body.s[p], rp_data->pdu.destination.len, rp_data->pdu.destination);
						if (rp_data->pdu.destination.len % 2 == 0) {
							p += rp_data->pdu.destination.len/2;
						} else {
							p += (rp_data->pdu.destination.len/2)+1;
						}
					}
				}else if (rp_data->pdu.msg_type == DELIVER) {
					// TP-OA
					rp_data->pdu.originating_address.len = body.s[p++];
					if (rp_data->pdu.originating_address.len > 0) {
						rp_data->pdu.originating_address_flags = (unsigned char)body.s[p++];
						rp_data->pdu.originating_address.s = pkg_malloc(rp_data->pdu.originating_address.len);
						DecodePhoneNumber(&body.s[p], rp_data->pdu.originating_address.len, rp_data->pdu.originating_address);
						if (rp_data->pdu.originating_address.len % 2 == 0) {
							p += rp_data->pdu.originating_address.len/2;
						} else {
							p += (rp_data->pdu.originating_address.len/2)+1;
						}
					}
				}

				rp_data->pdu.pid = (unsigned char)body.s[p++];

				if (rp_data->pdu.msg_type == SUBMIT || rp_data->pdu.msg_type == DELIVER) {
					rp_data->pdu.coding = (unsigned char)body.s[p++];
				}

				// 3GPP TS 03.40 9.2.2.2 SMS SUBMIT type
				// https://en.wikipedia.org/wiki/GSM_03.40
				if(rp_data->pdu.msg_type == SUBMIT){
					// 3GPP TS 03.40 9.2.3.3 TP Validity Period Format (TP VPF)
					switch (rp_data->pdu.flags & BITMASK_TP_VPF){
						case BITMASK_TP_VPF_RELATIVE:	// 3GPP TS 03.40 9.2.3.12.1 TP-VP (Relative format)
							rp_data->pdu.validity = (unsigned char)body.s[p++];
							break;
						case BITMASK_TP_VPF_ABSOLUTE:	// 3GPP TS 03.40 9.2.3.12.2 TP-VP (Absolute format)
							DecodeTime(&body.s[p], &rp_data->pdu.time);
							p += 7;
							break;
						case BITMASK_TP_VPF_ENHANCED:	// 3GPP TS 03.40 9.2.3.12.3 TP-VP (Enhanced format)
							p += 7;
							LM_WARN("3GPP TS 03.40 9.2.3.12.3 TP-VP (Enhanced format) is not supported\n");
							break;
						default:
							break;
					}
				} else if (rp_data->pdu.msg_type == DELIVER) {
					// TP-SCTS
					DecodeTime(&body.s[p], &rp_data->pdu.time);
					p += 7;
				}

				if (rp_data->pdu.msg_type == SUBMIT || rp_data->pdu.msg_type == DELIVER) {
					//TP-User-Data-Length and TP-User-Data
					len = (unsigned char)body.s[p++];
					int fill_bits = 0;
					if (len > 0) {
						if((unsigned char)rp_data->pdu.flags & BITMASK_TP_UDHI) { //TP-UDHI
							int udh_len = (unsigned char)body.s[p++];
							int udh_read = 0;

							if(rp_data->pdu.coding == 0) {
								//calcucate padding size for 7bit coding
								//udh_len + 1, because the length field itself should be included
								fill_bits = (7 - (udh_len + 1) % 7) % 7; //padding size is in bits!
							}

							// Check for malicious length, which might cause buffer overflow
							if(udh_len > body.len - p) {
								LM_ERR("TP-User-Data-Length is bigger than the remaining message buffer!\n");
								return -1;
							}

							//User-Data-Header
							tp_udh_inf_element_t* prev_ie = NULL;
							// IE 'Concatenated short messages, 8-bit reference number' should not be repeated
							int contains_8bit_refnum = 0;
							while(udh_read < udh_len) {
								tp_udh_inf_element_t* ie = pkg_malloc(sizeof(tp_udh_inf_element_t));
								if(ie == NULL) {
									LM_ERR("no more pkg\n");
									return -1;
								}
								memset(ie, 0, sizeof(tp_udh_inf_element_t));

								ie->identifier = (unsigned char)body.s[p++];
								ie->data.len = (unsigned char)body.s[p++];

								// Check for malicious length, which might cause buffer overflow
								if(udh_read + ie->data.len + 2 /* two octets are read so far */ > udh_len) {
									LM_ERR("IE Length for IE id %d is bigger than the remaining User-Data element!\n",
																										ie->identifier);
									pkg_free(ie);
									return -1;
								}

								if(ie->identifier == TP_UDH_IE_CONCAT_SM_8BIT_REF) {
									if(contains_8bit_refnum) {
										pkg_free(ie);
										LM_ERR("IE Concatenated Short Message 8bit Reference occurred more than once in UDH\n");
										return -1;
									}

									ie->concat_sm_8bit_ref.ref = body.s[p++];
									ie->concat_sm_8bit_ref.max_num_sm = body.s[p++];
									ie->concat_sm_8bit_ref.seq = body.s[p++];

									contains_8bit_refnum = 1;
								}
								else { /* Unsupported IE, save it as binary */
									ie->data.s = pkg_malloc(ie->data.len);
									if(ie->data.s == NULL) {
										pkg_free(ie);
										LM_ERR("no more pkg\n");
										return -1;
									}
									memset(ie->data.s, 0, ie->data.len);
									memcpy(ie->data.s, &body.s[p], ie->data.len);
									p += ie->data.len;
								}

								if(prev_ie == NULL) {
									rp_data->pdu.payload.header = ie;
								}
								else {
									prev_ie->next = ie;
								}

								prev_ie = ie;
								udh_read += (1 /* IE ID */ + 1 /* IE Len */ + ie->data.len /* IE data */);
							}

							// TS 23.040, Sec. 9.2.3.16
							// Coding: 7 Bit
							if (rp_data->pdu.coding == 0x00) {
								int udh_bit_len = (1 + udh_len) * 8; // add 1 octet for the udh length
								udh_bit_len += (7 - (udh_bit_len % 7));
								len -= (udh_bit_len / 7);
							}else{
								len -= (1 + udh_len); // add 1 octet for the udh length
							}
						}

						blen = 2 + len*4;
						rp_data->pdu.payload.sm.s = pkg_malloc(blen);
						if(rp_data->pdu.payload.sm.s==NULL) {
							LM_ERR("no more pkg\n");
							return -1;
						}
						memset(rp_data->pdu.payload.sm.s, 0, blen);
						// Coding: 7 Bit
						if (rp_data->pdu.coding == 0x00) {
							// We don't care about the extra used bytes here.
							rp_data->pdu.payload.sm.len = len;
							rp_data->pdu.payload.sm.len = gsm_to_ascii(&body.s[p], len, rp_data->pdu.payload.sm, fill_bits);
						} else {
							// Length is worst-case 2 * len (UCS2 is 2 Bytes, UTF8 is worst-case 4 Bytes)
							rp_data->pdu.payload.sm.len = 0;
							while (len > 0) {
								j = ((unsigned char)body.s[p] << 8) + (unsigned char)body.s[p + 1];
								p += 2;
								rp_data->pdu.payload.sm.len += ucs2_to_utf8(j, &rp_data->pdu.payload.sm.s[rp_data->pdu.payload.sm.len]);
								len -= 2;
							}
						}
					}
				}
			}
		}
	}

	return 1;
}

int dumpRPData(sms_rp_data_t * rpdata, int level) {
	if (rpdata) {
		LOG(level, "SMS-Message\n");
		LOG(level, "------------------------\n");
		LOG(level, "RP-Data\n");
		LOG(level, "  Type:                       %x\n", rpdata->msg_type);
		LOG(level, "  Reference:                  %x (%i)\n", rpdata->reference, rpdata->reference);
		LOG(level, "  Originator flags:           %x (%i)\n", rpdata->originator_flags, rpdata->originator_flags);
		LOG(level, "  Originator:                 %.*s (%i)\n", rpdata->originator.len, rpdata->originator.s, rpdata->originator.len);
		LOG(level, "  Destination flags:          %x (%i)\n", rpdata->destination_flags, rpdata->destination_flags);
		LOG(level, "  Destination:                %.*s (%i)\n", rpdata->destination.len, rpdata->destination.s, rpdata->destination.len);
		LOG(level, "T-PDU\n");
		LOG(level, "  Type:                       %x\n", rpdata->pdu.msg_type);
		LOG(level, "  Flags:                      %x (%i)\n", rpdata->pdu.flags, rpdata->pdu.flags);
		LOG(level, "  Reference:                  %x (%i)\n", rpdata->pdu.reference, rpdata->pdu.reference);

		LOG(level, "  Originating-Address flags:  %x (%i)\n", rpdata->pdu.originating_address_flags, rpdata->pdu.originating_address_flags);
		LOG(level, "  Originating-Address:        %.*s (%i)\n", rpdata->pdu.originating_address.len, rpdata->pdu.originating_address.s, rpdata->pdu.originating_address.len);
		LOG(level, "  Destination flags:          %x (%i)\n", rpdata->pdu.destination_flags, rpdata->pdu.destination_flags);
		LOG(level, "  Destination:                %.*s (%i)\n", rpdata->pdu.destination.len, rpdata->pdu.destination.s, rpdata->pdu.destination.len);

		LOG(level, "  Protocol:                   %x (%i)\n", rpdata->pdu.pid, rpdata->pdu.pid);
		LOG(level, "  Coding:                     %x (%i)\n", rpdata->pdu.coding, rpdata->pdu.coding);
		LOG(level, "  Validity:                   %x (%i)\n", rpdata->pdu.validity, rpdata->pdu.validity);

		LOG(level, "  Payload:                    %.*s (%i)\n", rpdata->pdu.payload.sm.len, rpdata->pdu.payload.sm.s, rpdata->pdu.payload.sm.len);
	}
	return 1;
}

/*******************************************************************
 * Implementation
 *******************************************************************/

/*
 * Creates the body for SMS-ACK from the current message
 */
int pv_sms_ack(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	str rp_data_ack = {0, 0};

	// Decode the 3GPP-SMS:
	if (decode_3gpp_sms(msg) != 1) {
		LM_ERR("Error getting/decoding RP-Data from request!\n");
		return -1;
	}

	// RP-Type (1) + RP-Ref (1) + RP-User-Data (Element-ID (1) + Length (9 => Msg-Type (1) + Parameter (1) + Service-Centre-Time (7)) = 13;
	rp_data_ack.len = 13;
	rp_data_ack.s = (char*)pkg_malloc(rp_data_ack.len);
	if (!rp_data_ack.s) {
		LM_ERR("Error allocating %d bytes!\n", rp_data_ack.len);
		return -1;
	}

	// Encode the data (RP-Data)
	// Always ACK NETWORK to MS
	rp_data_ack.s[0] = RP_ACK_NETWORK_TO_MS;
	// Take the reference from request:
	rp_data_ack.s[1] = rp_data->reference;
	// RP-Data-Element-ID
	rp_data_ack.s[2] = 0x41;
	// Length
	rp_data_ack.s[3] = 9;
	// PDU
	// SMS-SUBMIT-Report
	rp_data_ack.s[4] = SUBMIT;
	// Parameters (none)
	rp_data_ack.s[5] = 0x0;

	EncodeTime(&rp_data_ack.s[6]);

	return pv_get_strval(msg, param, res, &rp_data_ack);
}


/*
 * Creates the body for SMS-DELIVER from the constructed rp data
 */
int pv_sms_body(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	dumpRPData(rp_send_data, L_DBG);

	str sms_body = {0, 0};
	int buffer_size = 1024, lenpos = 0, i = 0, smstext_len_pos = 0;
	unsigned char udh_len = 0;
	struct ie_concat_sm_8bit_ref* concat = NULL;

	// We assume a maximum size of 1024 Bytes, to be verified.
	sms_body.s = (char*)pkg_malloc(buffer_size);
	if (!sms_body.s) {
		LM_ERR("Error allocating %i bytes!\n", buffer_size);
		return -1;
	}

	// Encode the data (RP-Data)
	sms_body.s[sms_body.len++] = rp_send_data->msg_type;
	sms_body.s[sms_body.len++] = rp_send_data->reference;
	lenpos = sms_body.len;
	sms_body.s[sms_body.len++] = 0x00;
	if (rp_send_data->originator.len > 0) {
		sms_body.s[sms_body.len++] = rp_send_data->originator_flags; // Type of number: ISDN/Telephony Numbering (E164), no extension
		i = EncodePhoneNumber(rp_send_data->originator, &sms_body.s[sms_body.len], buffer_size - sms_body.len);
		sms_body.s[lenpos] = i + 1;
		sms_body.len += i;
	}
	lenpos = sms_body.len;
	sms_body.s[sms_body.len++] = 0x00;
	if (rp_send_data->destination.len > 0) {
		sms_body.s[sms_body.len++] = rp_send_data->destination_flags; // Type of number: ISDN/Telephony Numbering (E164), no extension
		i = EncodePhoneNumber(rp_send_data->destination, &sms_body.s[sms_body.len], buffer_size - sms_body.len);
		sms_body.s[lenpos] = i + 1;
		sms_body.len += i;
	}
	// Store the position of the length for later usage:
	lenpos = sms_body.len;
	sms_body.s[sms_body.len++] = 0x00;

	///////////////////////////////////////////////////
	// T-PDU
	///////////////////////////////////////////////////
	sms_body.s[sms_body.len++] = rp_send_data->pdu.msg_type | rp_send_data->pdu.flags | 0x4; // We've always got no more messages to send.
	// Originating Address:
	sms_body.s[sms_body.len++] = rp_send_data->pdu.originating_address.len;
	sms_body.s[sms_body.len++] = rp_send_data->pdu.originating_address_flags;
	sms_body.len += EncodePhoneNumber(rp_send_data->pdu.originating_address, &sms_body.s[sms_body.len], buffer_size - sms_body.len);
	// Protocol ID
	sms_body.s[sms_body.len++] = rp_send_data->pdu.pid;
	// Encoding (0 => default 7 Bit)
	sms_body.s[sms_body.len++] = rp_send_data->pdu.coding;
	// Service-Center-Timestamp (always 7 octets)
	EncodeTime(&sms_body.s[sms_body.len]);
	sms_body.len += 7;
	smstext_len_pos = sms_body.len;
	sms_body.s[sms_body.len++] = rp_send_data->pdu.payload.sm.len;

	// Check for UDH
	concat = GetConcatShortMsg8bitRefIE(rp_send_data);
	if(concat->max_num_sm && concat->seq) {
		sms_body.s[sms_body.len++] = 5; // always 5 for TP_UDH_IE_CONCAT_SM_8BIT_REF
		sms_body.s[sms_body.len++] = TP_UDH_IE_CONCAT_SM_8BIT_REF;
		sms_body.s[sms_body.len++] = 3; // always 3 for TP_UDH_IE_CONCAT_SM_8BIT_REF
		sms_body.s[sms_body.len++] = concat->ref;
		sms_body.s[sms_body.len++] = concat->max_num_sm;
		sms_body.s[sms_body.len++] = concat->seq;
		udh_len = 6;
	}

	// Coding: 7 Bit
	if (rp_send_data->pdu.coding == 0x00) {
		int actual_text_size = 0;
		unsigned char paddingBits = (udh_len * 8 ) % 7;
		if (paddingBits) {
			paddingBits = 7 - paddingBits; 
		}

		i = ascii_to_gsm(rp_send_data->pdu.payload.sm, &sms_body.s[sms_body.len], buffer_size - sms_body.len, &actual_text_size, paddingBits);
		sms_body.len += i;
		sms_body.s[smstext_len_pos] = actual_text_size + udh_len;
	} else {
	// Coding: ucs2
		int i, ucs2, ucs2len = 0;
		const unsigned char* p_input = (unsigned char*)rp_send_data->pdu.payload.sm.s;
		const unsigned char* p_end = p_input;

		for (i = 0; i < rp_send_data->pdu.payload.sm.len;) {
			ucs2 = utf8_to_ucs2(p_input, &p_end);
			if (ucs2 < 0) {
				break;
			}

			sms_body.s[sms_body.len++] = (ucs2 >> 8) & 0xFF;
			sms_body.s[sms_body.len++] = ucs2 & 0xFF;

			ucs2len += 2;

			i += (p_end - p_input);
			p_input = p_end;
		}

		// Update the sms text len
		sms_body.s[smstext_len_pos] = (unsigned char)ucs2len + udh_len;
	}

	// Update the len of the PDU
	sms_body.s[lenpos] = (unsigned char)(sms_body.len - lenpos - 1);

	return pv_get_strval(msg, param, res, &sms_body);
}

int pv_get_sms(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	if (param==NULL) return -1;

	// Decode the 3GPP-SMS:
	if (decode_3gpp_sms(msg) != 1) {
		LM_ERR("Error getting/decoding RP-Data from request!\n");
		return -1;
	}

	switch(param->pvn.u.isname.name.n) {
		case SMS_RPDATA_TYPE:
			return pv_get_sintval(msg, param, res, (int)rp_data->msg_type);
		case SMS_RPDATA_REFERENCE:
			return pv_get_sintval(msg, param, res, (int)rp_data->reference);
		case SMS_RPDATA_ORIGINATOR:
			return pv_get_strval(msg, param, res, &rp_data->originator);
		case SMS_RPDATA_DESTINATION:
			return pv_get_strval(msg, param, res, &rp_data->destination);
		case SMS_TPDU_TYPE:
			return  pv_get_sintval(msg, param, res, (int)rp_data->pdu.msg_type);
		case SMS_TPDU_FLAGS:
			return  pv_get_sintval(msg, param, res, (int)rp_data->pdu.flags);
		case SMS_TPDU_CODING:
			return  pv_get_sintval(msg, param, res, (int)rp_data->pdu.coding);
		case SMS_TPDU_PROTOCOL:
			return  pv_get_sintval(msg, param, res, (int)rp_data->pdu.pid);
		case SMS_TPDU_VALIDITY:
			return  pv_get_sintval(msg, param, res, (int)rp_data->pdu.validity);
		case SMS_TPDU_REFERENCE:
			return  pv_get_sintval(msg, param, res, (int)rp_data->pdu.reference);
		case SMS_TPDU_PAYLOAD:
			return pv_get_strval(msg, param, res, &rp_data->pdu.payload.sm);
		case SMS_TPDU_DESTINATION:
			return pv_get_strval(msg, param, res, &rp_data->pdu.destination);
		case SMS_TPDU_ORIGINATING_ADDRESS:
			return pv_get_strval(msg, param, res, &rp_data->pdu.originating_address);
		case SMS_UDH_CONCATSM_REF: {
			tp_udh_inf_element_t* ie = rp_data->pdu.payload.header;
			while(ie) {
				if(ie->identifier == TP_UDH_IE_CONCAT_SM_8BIT_REF)
					return pv_get_uintval(msg, param, res, (unsigned int)ie->concat_sm_8bit_ref.ref);
				ie = ie->next;
			}
			return -1;
		}
		case SMS_UDH_CONCATSM_MAX_NUM_SM: {
			tp_udh_inf_element_t* ie = rp_data->pdu.payload.header;
			while(ie) {
				if(ie->identifier == TP_UDH_IE_CONCAT_SM_8BIT_REF)
					return pv_get_uintval(msg, param, res, (unsigned int)ie->concat_sm_8bit_ref.max_num_sm);
				ie = ie->next;
			}
			return -1;
		}
		case SMS_UDH_CONCATSM_SEQ: {
			tp_udh_inf_element_t* ie = rp_data->pdu.payload.header;
			while(ie) {
				if(ie->identifier == TP_UDH_IE_CONCAT_SM_8BIT_REF)
					return pv_get_uintval(msg, param, res, (unsigned int)ie->concat_sm_8bit_ref.seq);
				ie = ie->next;
			}
			return -1;
		}
		case SMS_TPDU_ORIGINATING_ADDRESS_FLAGS:
			return pv_get_sintval(msg, param, res, (int)rp_data->pdu.originating_address_flags);
		case SMS_TPDU_DESTINATION_FLAGS:
			return pv_get_sintval(msg, param, res, (int)rp_data->pdu.destination_flags);
		case SMS_RPDATA_ORIGINATOR_FLAGS:
			return pv_get_sintval(msg, param, res, (int)rp_data->originator_flags);
		case SMS_RPDATA_DESTINATION_FLAGS:
			return pv_get_sintval(msg, param, res, (int)rp_data->destination_flags);
	}
	return 0;
}

int pv_set_sms(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val) {
	if (param==NULL)
		return -1;

	if (!rp_send_data) {
		rp_send_data = (sms_rp_data_t*)pkg_malloc(sizeof(struct _sms_rp_data));
		if (!rp_send_data) {
			LM_ERR("Error allocating %lu bytes!\n", (unsigned long)sizeof(struct _sms_rp_data));
			return -1;
		}
		// Initialize structure:
		memset(rp_send_data, 0, sizeof(struct _sms_rp_data));
		rp_send_data->pdu.originating_address_flags = 0x91; // Type of number: ISDN/Telephony Numbering (E164), no extension
		rp_send_data->pdu.destination_flags = 0x91;
		rp_send_data->originator_flags = 0x91;
		rp_send_data->destination_flags = 0x91;
	}

	switch(param->pvn.u.isname.name.n) {
		case SMS_ALL:
			freeRP_DATA(rp_send_data);
			// Initialize structure:
			memset(rp_send_data, 0, sizeof(struct _sms_rp_data));
			rp_send_data->pdu.originating_address_flags = 0x91; // Type of number: ISDN/Telephony Numbering (E164), no extension
			rp_send_data->pdu.destination_flags = 0x91;
			rp_send_data->originator_flags = 0x91;
			rp_send_data->destination_flags = 0x91;
			break;
		case SMS_RPDATA_TYPE:
			if (val == NULL) {
				rp_send_data->msg_type = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->msg_type = (unsigned char)val->ri;
			break;
		case SMS_RPDATA_REFERENCE:
			if (val == NULL) {
				rp_send_data->reference = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->reference = (unsigned char)val->ri;
			break;
		case SMS_RPDATA_ORIGINATOR:
			if (rp_send_data->originator.s) {
				pkg_free(rp_send_data->originator.s);
				rp_send_data->originator.s = 0;
				rp_send_data->originator.len = 0;
			}
			if (val == NULL)
				return 0;
			if (!(val->flags&PV_VAL_STR)) {
				LM_ERR("Invalid type\n");
				return -1;
			}
			rp_send_data->originator.s = pkg_malloc(val->rs.len);
			if(rp_send_data->originator.s==NULL) {
				LM_ERR("no more pkg\n");
				return -1;
			}
			rp_send_data->originator.len = val->rs.len;
			memcpy(rp_send_data->originator.s, val->rs.s, val->rs.len);
			break;
		case SMS_RPDATA_DESTINATION:
			if (rp_send_data->destination.s) {
				pkg_free(rp_send_data->destination.s);
				rp_send_data->destination.s = 0;
				rp_send_data->destination.len = 0;
			}
			if (val == NULL)
				return 0;
			if (!(val->flags&PV_VAL_STR)) {
				LM_ERR("Invalid type\n");
				return -1;
			}
			rp_send_data->destination.s = pkg_malloc(val->rs.len);
			if(rp_send_data->destination.s==NULL) {
				LM_ERR("no more pkg\n");
				return -1;
			}
			rp_send_data->destination.len = val->rs.len;
			memcpy(rp_send_data->destination.s, val->rs.s, val->rs.len);
			break;
		case SMS_TPDU_TYPE:
			if (val == NULL) {
				rp_send_data->pdu.msg_type = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->pdu.msg_type = (unsigned char)val->ri;
			break;

		case SMS_TPDU_FLAGS:
			if (val == NULL) {
				rp_send_data->pdu.flags = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->pdu.flags = (unsigned char)val->ri;
			break;
		case SMS_TPDU_CODING:
			if (val == NULL) {
				rp_send_data->pdu.coding = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->pdu.coding = (unsigned char)val->ri;
			break;
		case SMS_TPDU_PROTOCOL:
			if (val == NULL) {
				rp_send_data->pdu.pid = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->pdu.pid = (unsigned char)val->ri;
			break;
		case SMS_TPDU_REFERENCE:
			if (val == NULL) {
				rp_send_data->pdu.reference = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->pdu.reference = (unsigned char)val->ri;
			break;
		case SMS_TPDU_VALIDITY:
			if (val == NULL) {
				rp_send_data->pdu.validity = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->pdu.validity = (unsigned char)val->ri;
			break;
		case SMS_TPDU_PAYLOAD:
			if (rp_send_data->pdu.payload.sm.s) {
				pkg_free(rp_send_data->pdu.payload.sm.s);
				rp_send_data->pdu.payload.sm.s = 0;
				rp_send_data->pdu.payload.sm.len = 0;
			}
			if (val == NULL)
				return 0;
			if (!(val->flags&PV_VAL_STR)) {
				LM_ERR("Invalid type\n");
				return -1;
			}
			rp_send_data->pdu.payload.sm.s = pkg_malloc(val->rs.len);
			if(rp_send_data->pdu.payload.sm.s==NULL) {
				LM_ERR("no more pkg\n");
				return -1;
			}
			rp_send_data->pdu.payload.sm.len = val->rs.len;
			memcpy(rp_send_data->pdu.payload.sm.s, val->rs.s, val->rs.len);
			break;
		case SMS_TPDU_DESTINATION:
			if (rp_send_data->pdu.destination.s) {
				pkg_free(rp_send_data->pdu.destination.s);
				rp_send_data->pdu.destination.s = 0;
				rp_send_data->pdu.destination.len = 0;
			}
			if (val == NULL)
				return 0;
			if (!(val->flags&PV_VAL_STR)) {
				LM_ERR("Invalid type\n");
				return -1;
			}
			rp_send_data->pdu.destination.s = pkg_malloc(val->rs.len);
			if(rp_send_data->pdu.destination.s==NULL) {
				LM_ERR("no more pkg\n");
				return -1;
			}
			rp_send_data->pdu.destination.len = val->rs.len;
			memcpy(rp_send_data->pdu.destination.s, val->rs.s, val->rs.len);
			break;
		case SMS_TPDU_ORIGINATING_ADDRESS:
			if (rp_send_data->pdu.originating_address.s) {
				pkg_free(rp_send_data->pdu.originating_address.s);
				rp_send_data->pdu.originating_address.s = 0;
				rp_send_data->pdu.originating_address.len = 0;
			}
			if (val == NULL)
				return 0;
			if (!(val->flags&PV_VAL_STR)) {
				LM_ERR("Invalid type\n");
				return -1;
			}
			rp_send_data->pdu.originating_address.s = pkg_malloc(val->rs.len);
			if(rp_send_data->pdu.originating_address.s==NULL) {
				LM_ERR("no more pkg\n");
				return -1;
			}
			rp_send_data->pdu.originating_address.len = val->rs.len;
			memcpy(rp_send_data->pdu.originating_address.s, val->rs.s, val->rs.len);
			break;
		case SMS_UDH_CONCATSM_REF: {
			if (val == NULL)
				return 0;
			if (!(val->flags&PV_VAL_INT)) {
				LM_ERR("Invalid type\n");
				return -1;
			}
			struct ie_concat_sm_8bit_ref* concat = GetConcatShortMsg8bitRefIE(rp_send_data);
			if(concat == NULL)
				return -1;

			concat->ref = (unsigned char)val->ri;
			break;
		}
		case SMS_UDH_CONCATSM_MAX_NUM_SM: {
			if (val == NULL)
				return 0;
			if (!(val->flags&PV_VAL_INT)) {
				LM_ERR("Invalid type\n");
				return -1;
			}
			struct ie_concat_sm_8bit_ref* concat = GetConcatShortMsg8bitRefIE(rp_send_data);
			if(concat == NULL)
				return -1;

			concat->max_num_sm = (unsigned char)val->ri;
			break;
		}
		case SMS_UDH_CONCATSM_SEQ: {
			if (val == NULL)
				return 0;
			if (!(val->flags&PV_VAL_INT)) {
				LM_ERR("Invalid type\n");
				return -1;
			}
			struct ie_concat_sm_8bit_ref* concat = GetConcatShortMsg8bitRefIE(rp_send_data);
			if(concat == NULL)
				return -1;

			concat->seq = (unsigned char)val->ri;
			break;
		}
		case SMS_TPDU_ORIGINATING_ADDRESS_FLAGS: {
			if (val == NULL) {
				rp_send_data->pdu.originating_address_flags = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->pdu.originating_address_flags = (unsigned char)val->ri;
			break;
		}
		case SMS_TPDU_DESTINATION_FLAGS: {
			if (val == NULL) {
				rp_send_data->pdu.destination_flags = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->pdu.destination_flags = (unsigned char)val->ri;
			break;
		}
		case SMS_RPDATA_ORIGINATOR_FLAGS: {
			if (val == NULL) {
				rp_send_data->originator_flags = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->originator_flags = (unsigned char)val->ri;
			break;
		}
		case SMS_RPDATA_DESTINATION_FLAGS: {
			if (val == NULL) {
				rp_send_data->destination_flags = 0;
				return 0;
			}
			if (!(val->flags & PV_VAL_INT)) {
				LM_ERR("Invalid value type\n");
				return -1;
			}
			rp_send_data->destination_flags = (unsigned char)val->ri;
			break;
		}
	}
	return 0;
}

int pv_parse_rpdata_name(pv_spec_p sp, str *in) {
	if (sp==NULL || in==NULL || in->len<=0) return -1;

	switch(in->len) {
		case 3:
			if (strncmp(in->s, "all", 3) == 0) sp->pvp.pvn.u.isname.name.n = SMS_ALL;
			else goto error;
			break;
		case 4:
			if (strncmp(in->s, "type", 4) == 0) sp->pvp.pvn.u.isname.name.n = SMS_RPDATA_TYPE;
			else goto error;
			break;
		case 9:
			if (strncmp(in->s, "reference", 9) == 0) sp->pvp.pvn.u.isname.name.n = SMS_RPDATA_REFERENCE;
			else goto error;
			break;
		case 10:
			if (strncmp(in->s, "originator", 10) == 0) sp->pvp.pvn.u.isname.name.n = SMS_RPDATA_ORIGINATOR;
			else goto error;
			break;
		case 11:
			if (strncmp(in->s, "destination", 11) == 0) sp->pvp.pvn.u.isname.name.n = SMS_RPDATA_DESTINATION;
			else goto error;
			break;
		case 12:
			if (strncmp(in->s, "origen_flags", 12) == 0) sp->pvp.pvn.u.isname.name.n = SMS_RPDATA_ORIGINATOR_FLAGS;
			else goto error;
			break;
		case 17:
			if (strncmp(in->s, "destination_flags", 17) == 0) sp->pvp.pvn.u.isname.name.n = SMS_RPDATA_DESTINATION_FLAGS;
			else goto error;
			break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown uac_req name %.*s\n", in->len, in->s);
	return -1;
}

int pv_parse_tpdu_name(pv_spec_p sp, str *in) {
	if (sp==NULL || in==NULL || in->len<=0) return -1;

	switch(in->len) {
		case 3:
			if (strncmp(in->s, "all", 3) == 0) sp->pvp.pvn.u.isname.name.n = SMS_ALL;
			else goto error;
			break;
		case 4:
			if (strncmp(in->s, "type", 4) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_TYPE;
			else goto error;
			break;
		case 5:
			if (strncmp(in->s, "flags", 5) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_FLAGS;
			else if (strncmp(in->s, "mp_id", 5) == 0) sp->pvp.pvn.u.isname.name.n = SMS_UDH_CONCATSM_REF;
			else goto error;
			break;
		case 6:
			if (strncmp(in->s, "coding", 6) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_CODING;
			else if (strncmp(in->s, "origen", 6) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_ORIGINATING_ADDRESS;
			else goto error;
			break;
		case 7:
			if (strncmp(in->s, "payload", 7) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_PAYLOAD;
			else goto error;
			break;
		case 8:
			if (strncmp(in->s, "protocol", 8) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_PROTOCOL;
			else if (strncmp(in->s, "validity", 8) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_VALIDITY;
			else if (strncmp(in->s, "mp_parts", 8) == 0) sp->pvp.pvn.u.isname.name.n = SMS_UDH_CONCATSM_MAX_NUM_SM;
			else goto error;
			break;
		case 9:
			if (strncmp(in->s, "reference", 9) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_REFERENCE;
			else goto error;
			break;
		case 11:
			if (strncmp(in->s, "destination", 11) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_DESTINATION;
			else if (strncmp(in->s, "mp_part_num", 11) == 0) sp->pvp.pvn.u.isname.name.n = SMS_UDH_CONCATSM_SEQ;
			else goto error;
			break;
		case 12:
			if (strncmp(in->s, "origen_flags", 12) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_ORIGINATING_ADDRESS_FLAGS;
			else goto error;
			break;
		case 17:
			if (strncmp(in->s, "destination_flags", 17) == 0) sp->pvp.pvn.u.isname.name.n = SMS_TPDU_DESTINATION_FLAGS;
			else goto error;
			break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown pdu name %.*s\n", in->len, in->s);
	return -1;
}

/*
 * Dumps the content of the SMS-Message:
 */
int smsdump(struct sip_msg *msg, char *str1, char *str2) {
	// Decode the 3GPP-SMS:
	if (decode_3gpp_sms(msg) != 1) {
		LM_ERR("Error getting/decoding RP-Data from request!\n");
		return -1;
	}

	return dumpRPData(rp_data, L_DBG);
}

int isRPDATA(struct sip_msg *msg, char *str1, char *str2) {
	// Decode the 3GPP-SMS:
	if (decode_3gpp_sms(msg) != 1) {
		LM_ERR("Error getting/decoding RP-Data from request!\n");
		return -1;
	}
	if ((rp_data->msg_type == RP_DATA_MS_TO_NETWORK) || (rp_data->msg_type == RP_DATA_NETWORK_TO_MS))
		return 1;
	else
		return -1;
}

