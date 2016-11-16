/*
 * ss7 module - ISUP parsed
 *
 * Copyright (C) 2016 Holger Hans Peter Freyther (help@moiji-mobile.com)
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

#include "isup_parsed.h"
#include "isup_generated.h"

#include "../../dprint.h"
#include "../../endianness.h"

#include <arpa/inet.h>

#include <string.h>

typedef void (*visit)(uint8_t type, const uint8_t *data, uint8_t len, struct isup_state *ptrs);

struct key_val {
	uint8_t		key;
	const char	*value;
};

static const struct key_val itu_cause_class[] = {
	{ 0x00, "normal event" },
	{ 0x01, "normal event" },
	{ 0x02, "resource unavailable" },
	{ 0x03, "service or option not available" },
	{ 0x04, "service or option not implemented" },
	{ 0x05, "invalid message" },
	{ 0x06, "protocol error" },
	{ 0x07, "interworking"} ,
	{ 0, },
};

static const struct key_val cause_value[] = {
	{ 1,	"Unallocated (unassigned) number" },
	{ 2,	"No route to specified transit network" },
	{ 3,	"No route to destination" },
	{ 4,	"Send special information tone" },
	{ 5,	"Misdialled trunk prefix" },
	{ 6,	"Channel unacceptable" },
	{ 7,	"Call awarded and being delivered in an established channel" },
	{ 8,	"Preemption" },
	{ 9,	"Preemption - circuit reserved for reuse" },
	{ 14,	"QoR: ported number" }, /* Add.1 */
	{ 16,	"Normal call clearing" },
	{ 17,	"User busy" },
	{ 18,	"No user responding" },
	{ 19,	"No answer from user (user alerted)" },
	{ 20,	"Subscriber absent" },
	{ 21,	"Call rejected" },
	{ 22,	"Number changed" },
	{ 23,	"Redirection to new destination" },
	{ 24,	"Call rejected due to failure at the destination" }, /* Amd.1 */
	{ 25,	"Exchange routing error" },
	{ 26,	"Non-selected user clearing" },
	{ 27,	"Destination out of order" },
	{ 28,	"Invalid number format (address incomplete)" },
	{ 29,	"Facility rejected" },
	{ 30,	"Response to STATUS ENQUIRY" },
	{ 31,	"Normal, unspecified" },
	{ 34,	"No circuit/channel available" },
	{ 38,	"Network out of order" },
	{ 39,	"Permanent frame mode connection out of service" },
	{ 40,	"Permanent frame mode connection operational" },
	{ 41,	"Temporary failure" },
	{ 42,	"Switching equipment congestion" },
	{ 43,	"Access information discarded" },
	{ 44,	"Requested circuit/channel not available" },
	{ 46,	"Precedence call blocked" },
	{ 47,	"Resource unavailable, unspecified" },
	{ 49,	"Quality of service not available" },
	{ 50,	"Requested facility not subscribed" },
	{ 53,	"Outgoing calls barred within CUG" },
	{ 55,	"Incoming calls barred within CUG" },
	{ 57,	"Bearer capability not authorized" },
	{ 58,	"Bearer capability not presently available" },
	{ 62,	"Inconsistency in designated outgoing access information and subscriber class" },
	{ 63,	"Service or option not available, unspecified" },
	{ 65,	"Bearer capability not implemented" },
	{ 66,	"Channel type not implemented" },
	{ 69,	"Requested facility not implemented" },
	{ 70,	"Only restricted digital information bearer capability is available" },
	{ 79,	"Service or option not implemented, unspecified" },
	{ 81,	"Invalid call reference value" },
	{ 82,	"Identified channel does not exist" },
	{ 83,	"A suspended call exists, but this call identity does not" },
	{ 84,	"Call identity in use" },
	{ 85,	"No call suspended" },
	{ 86,	"Call having the requested call identity has been cleared" },
	{ 87,	"User not member of CUG" },
	{ 88,	"Incompatible destination" },
	{ 90,	"Non-existent CUG" },
	{ 91,	"Invalid transit network selection" },
	{ 95,	"Invalid message, unspecified" },
	{ 96,	"Mandatory information element is missing" },
	{ 97,	"Message type non-existent or not implemented" },
	{ 98,	"Message not compatible with call state or message type non-existent or not implemented" },
	{ 99,	"Information element /parameter non-existent or not implemented" },
	{ 100,	"Invalid information element contents" },
	{ 101,	"Message not compatible with call state" },
	{ 102,	"Recovery on timer expiry" },
	{ 103,	"Parameter non-existent or not implemented, passed on" },
	{ 110,	"Message with unrecognized parameter, discarded" },
	{ 111,	"Protocol error, unspecified" },
	{ 127,	"Interworking, unspecified" },
	{ 0, },
};

static const struct key_val cause_std[] = {
	{ 0x00, "ITU-T" },
	{ 0x01, "ISO/IEC" },
	{ 0x02, "National" },
	{ 0x03, "Specific" },
	{ 0, },
};

static const struct key_val cause_location[] = {
	{ 0x0, "user (U)" },
	{ 0x01, "private network serving the local user (LPN)" },
	{ 0x02, "public network serving the local user (LN)" },
	{ 0x03, "transit network (TN)" },
	{ 0x04, "public network serving the remote user (RLN)" },
	{ 0x05, "private network serving the remote user (RPN)" },
	{ 0x07, "international network (INTL)" },
	{ 0x0A, "network beyond interworking point (BI)" },
	{ 0, },
};

static const struct key_val event_info[] = {
	{ 0x01, "ALERTING" },
	{ 0x02, "PROGRESS" },
	{ 0x03, "in-band" },
	{ 0x04, "call forwarded on busy" },
	{ 0x05, "call forwarded on no reply" },
	{ 0x06, "call forwarded unconditional" },
	{ 0, },
};

static const char *lookup(const struct key_val *table, const uint8_t val, const char *deflt)
{
	while (1) {
		if (!table->value)
			return deflt;
		if (table->key == val)
			return table->value;
		table += 1;
	}
}

static inline char from_bcd(const uint8_t item) {
	if (item >= 0x0A)
		return 'A' + (item - 0x0A);
	return '0' + item;
}

static inline void decode_bcd(char *dest, const uint8_t *data, size_t len, int odd)
{
	int i;

	for (i = 0; i < len; ++i) {
		uint8_t lo = data[i] & 0x0F;
		uint8_t hi = (data[i] & 0xF0) >> 4;

		*dest++ = from_bcd(lo);

		/* ignore the last digit */
		if (i + 1 == len && odd)
			break;
		*dest++ = from_bcd(hi);
	}
	*dest = '\0';
}

static void append_e164(srjson_doc_t *doc, const char *name, const uint8_t *data, uint8_t len)
{
	char num[17] = { 0, };
	srjson_t *obj;
	int odd;

	/* at least TON/NPI in there */
	if (len < 2) {
		LM_ERR("Too short %s %u\n", name, len);
		return;
	}

	/* E.164 is limited to 15 digits so 8 octets */
	if (len - 2 > 8) {
		LM_ERR("Too big %s %u\n", name, len);
		return;
	}

	obj = srjson_CreateObject(doc);
	if (!obj) {
		LM_ERR("Can not allocate json object for %s\n", name);
		return;
	}

	odd = !!(data[0] & 0x80);
	srjson_AddNumberToObject(doc, obj, "ton", data[0] & 0x7F);
	srjson_AddNumberToObject(doc, obj, "npi", (data[1] >> 4) & 0x07);

	decode_bcd(num, &data[2], len - 2, odd);
	srjson_AddStringToObject(doc, obj, "num", num);

	srjson_AddItemToObject(doc, doc->root, name, obj);
}

static void append_itu_cause(srjson_doc_t *doc, srjson_t *obj, const uint8_t cause)
{
	/* ignore the "extension bit" */
	uint8_t itu_class = (cause & 0x60) >> 5;
	srjson_AddStringToObject(doc, obj, "itu_class_name", lookup(itu_cause_class, itu_class, "Unknown"));
	srjson_AddNumberToObject(doc, obj, "itu_class_num", itu_class);
	srjson_AddStringToObject(doc, obj, "itu_cause_name", lookup(cause_value, cause & 0x7F, "Unknown"));
	srjson_AddNumberToObject(doc, obj, "itu_cause_num", cause & 0x7F);
}

static void append_cause(srjson_doc_t *doc, const char *name, const uint8_t *data, uint8_t len)
{
	uint8_t std_loc;
	uint8_t cause_val;
	const char *std, *loc;
	int is_itu = 0;
	srjson_t *obj;

	if (len < 2) {
		LM_ERR("Not enough data for cause\n");
		return;
	}

	obj = srjson_CreateObject(doc);
	if (!obj) {
		LM_ERR("Can not allocate json object for %s\n", name);
		return;
	}

	std_loc = data[0];
	cause_val = data[1];

	is_itu = (std_loc & 0x60) == 0;
	std = lookup(cause_std, (std_loc & 0x60) >> 5, "Unknown");
	srjson_AddNumberToObject(doc, obj, "standard_num", ((std_loc & 0x60) >> 5));
	srjson_AddStringToObject(doc, obj, "standard_name", std);


	loc = lookup(cause_location, std_loc & 0x0F, "Unknown");
	srjson_AddNumberToObject(doc, obj, "location_num", (std_loc & 0x0F));
	srjson_AddStringToObject(doc, obj, "location_name", loc);

	if (is_itu)
		append_itu_cause(doc, obj, cause_val);

	srjson_AddItemToObject(doc, doc->root, name, obj);
}

static void append_event_information(srjson_doc_t *doc, const char *name, const uint8_t *data, uint8_t len)
{
	const char *event_str;
	const char *pres_str;
	srjson_t *obj;

	if (len < 1) {
		LM_ERR("Not enough data for event information\n");
		return;
	}

	obj = srjson_CreateObject(doc);
	if (!obj) {
		LM_ERR("Can not allocate json object for %s\n", name);
		return;
	}

	event_str = lookup(event_info, data[0] & 0x7F, "spare");
	if (data[0] & 0x80)
		pres_str = "presentation restricted";
	else
		pres_str = "no indication";

	srjson_AddNumberToObject(doc, obj, "event_num", data[0]);
	srjson_AddStringToObject(doc, obj, "event_str", event_str);
	srjson_AddStringToObject(doc, obj, "presentation_str", pres_str);

	srjson_AddItemToObject(doc, doc->root, name, obj);
}

static void isup_visitor(uint8_t type, const uint8_t *data, uint8_t len, struct isup_state *ptrs)
{
	switch (type) {
	case ISUPCalledPartyNumber:
		append_e164(ptrs->json, "called_number", data, len);
		break;
	case ISUPCallingPartyNumber:
		append_e164(ptrs->json, "calling_number", data, len);
		break;
	case ISUPCauseIndicators:
		append_cause(ptrs->json, "cause", data, len);
		break;
	case ISUPEventInformation:
		append_event_information(ptrs->json, "event", data, len);
		break;
	}
}

static uint16_t parse_cic(const uint8_t *data)
{
	uint16_t cic;
	memcpy(&cic, &data[0], sizeof(cic));
#ifdef __IS_LITTLE_ENDIAN
	return cic;
#else
	return bswap16(cic);
#endif
}

static int do_parse(const uint8_t *data, size_t len, visit visitor, struct isup_state *ptr)
{
	const struct isup_msg *msg_class;
	const uint8_t *ptrs;
	size_t ptrs_size;
	size_t left;

	if (len < 3) {
		LM_ERR("ISUP message too short %zu\n", len);
		return -1;
	}

	/* extract the basics */
	srjson_AddNumberToObject(ptr->json, ptr->json->root, "cic", parse_cic(data));
	srjson_AddNumberToObject(ptr->json, ptr->json->root, "msg_type", data[2]);

	msg_class = &isup_msgs[data[2]];
	if (!msg_class->name) {
		LM_ERR("ISUP message not known %d\n", data[2]);
		return -2;
	}
	srjson_AddStringToObject(ptr->json, ptr->json->root, "msg_name", msg_class->name);
	data += 3;
	left = len - 3;

	/*
	 * 1.) Fixed size mandatory elements!
	 * 2.) Area with pointers to variable and optional
	 * 3.) Variable elements pointed
	 * 4.) Optional ones..
	 */
	if (msg_class->fixed_ies) {
		const struct isup_ie_fixed *fixed = msg_class->fixed_ies;
		while (1) {
			if (!fixed->name)
				break;

			if (left < fixed->len) {
				LM_ERR("ISUP fixed too short %zu vs. %un", left, fixed->len);
				return -3;
			}

			/* handle the type */
			visitor(fixed->type, data, fixed->len, ptr);

			/* move forward */
			data += fixed->len;
			left -= fixed->len;
			fixed += 1;
		}
	}

	/* now we reached pointers to variable and optional */
	ptrs = data;
	ptrs_size = left;


	/* consume variables */
	if (msg_class->variable_ies) {
		const struct isup_ie_variable *variable = msg_class->variable_ies;

		while (1) {
			const uint8_t *ie_data;
			size_t ie_left;
			uint8_t ie_len;

			if (!variable->name)
				break;

			if (ptrs_size < 1) {
				LM_ERR("ISUP no space for ptr %zu\n", ptrs_size);
				return -1;
			}

			ie_left = ptrs_size;
			ie_data = ptrs;
			if (ie_left < ie_data[0]) {
				LM_ERR("ISUP no space for len %zu vs. %u\n", ie_left, ie_data[0]);
				return -1;
			}
			ie_left -= ie_data[0];
			ie_data += ie_data[0];

			ie_len = ie_data[0];
			if (ie_left < ie_len + 1) {
				LM_ERR("ISUP no space for data %zu vs. %u\n", ie_left, ie_len + 1);
				return -1;
			}

			visitor(variable->type, &ie_data[1], ie_len, ptr);

			/* move forward */
			ptrs_size -= 1;
			ptrs += 1;
			variable += 1;
		}
	}

	if (msg_class->has_optional) {
		size_t opt_left = ptrs_size;
		const uint8_t *opt_data = ptrs;

		if (opt_left < 1) {
			LM_ERR("ISUP no space for optional ptr\n");
			return -1;
		}
		if (opt_left < opt_data[0]) {
			LM_ERR("ISUP optional beyond msg %zu vs. %u\n", opt_left, opt_data[0]);
			return -1;
		}

		opt_left -= opt_data[0];
		opt_data += opt_data[0];

		while (opt_left > 0) {
			uint8_t ie = opt_data[0];
			size_t len;
			opt_data += 1;
			opt_left -= 1;

			if (ie == ISUPEndOfOptionalParameters)
				break;

			if (opt_left < 1) {
				LM_ERR("ISUP no space for len %zu\n", opt_left);
				return -1;
			}
			len = opt_data[0];
			opt_left -= 1;
			opt_data += 1;

			if (opt_left < len) {
				LM_ERR("ISUP no space optional data %zu vs. %zu\n", opt_left, len);
				return -1;
			}

			visitor(ie, opt_data, len, ptr);

			opt_data += len;
			opt_left -= len;
		}
	}

	return 0;
}

int isup_parse(const uint8_t *data, size_t len, struct isup_state *ptr)
{
	return do_parse(data, len, isup_visitor, ptr);
}
