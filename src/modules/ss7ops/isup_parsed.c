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

#include "../../core/dprint.h"
#include "../../core/endianness.h"

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

static const struct key_val nai_vals[] = {
	{ 0x00, "spare" },
	{ 0x01, "subscriber number (national use)" },
	{ 0x02, "unknown (national use)" },
	{ 0x03, "national (significant) number" },
	{ 0x04, "international number" },
	{ 0x05, "network-specific number (national use)" },
	{ 0x06, "network routing number in national (significant) number format (national use)" },
	{ 0x07, "network routing number in network-specific number format (national use)" },
	{ 0x08, "network routing number concatenated with Called Directory Number (national use)" },
	{ 0, },
};

static const struct key_val inn_vals[] = {
	{ 0x00, "routing to internal network number allowed" },
	{ 0x01, "routing to internal network number not allowed" },
	{ 0, },
};

static const struct key_val ni_vals[] = {
	{ 0x00, "complete" },
	{ 0x01, "incomplete" },
	{ 0, },
};

static const struct key_val npi_vals[] = {
	{ 0x00, "spare" },
	{ 0x01, "ISDN (Telephony) numbering plan (ITU-T Recommendation E.164)" },
	{ 0x02, "spare" },
	{ 0x03, "Data numbering plan (ITU-T Recommendation X.121) (national use)" },
	{ 0x04, "Telex numbering plan (ITU-T Recommendation F.69) (national use)" },
	{ 0x05, "reserved for national use" },
	{ 0x06, "reserved for national use" },
	{ 0, },
};

static const struct key_val restrict_vals[] = {
	{ 0x00, "presentation allowed" },
	{ 0x01, "presentation restricted" },
	{ 0x02, "address not available (Note 1) (national use)" },
	{ 0x03, "reserved for restriction by the network" },
	{ 0, },
};

static const struct key_val screened_vals[] = {
	{ 0x00,	"reserved (Note 2)" },
	{ 0x01, "user provided, verified and passed" },
	{ 0x02, "reserved (Note 2)" },
	{ 0x03, "network provided" },
	{ 0, },
};

static const struct key_val calling_cat_vals[] = {
	{ 0x00, "calling party's category unknown at this time (national use)" },
	{ 0x01, "operator, language French" },
	{ 0x02, "operator, language English" },
	{ 0x03, "operator, language German" },
	{ 0x04, "operator, language Russian" },
	{ 0x05, "operator, language Spanish" },
	{ 0x09, "reserved (see ITU-T Recommendation Q.104) (Note) (national use)" },
	{ 0x0A, "ordinary calling subscriber" },
	{ 0x0B, "calling subscriber with priority" },
	{ 0x0C, "data call (voice band data)" },
	{ 0x0D, "test call" },
	{ 0x0E, "spare" },
	{ 0x0F, "payphone" },
	{ 0, },
};

static const struct key_val nci_sat_vals[] = {
	{ 0x00, "no satellite circuit in the connection" },
	{ 0x01, "one satellite circuit in the connection" },
	{ 0x02, "two satellite circuits in the connection" },
	{ 0x03, "spare" },
	{ 0, },
};

static const struct key_val nci_con_vals[] = {
	{ 0x00, "continuity check not required" },
	{ 0x01, "continuity check required on this circuit" },
	{ 0x02, "continuity check performed on a previous circuit" },
	{ 0x03, "spare" },
	{ 0, },
};

static const struct key_val nci_echo_vals[] = {
	{ 0x00, "outgoing echo control device not included" },
	{ 0x01, "outgoing echo control device included" },
	{ 0, },
};

/* Forward call indicators National/international call indicator */
static const struct key_val fwc_nic_vals[] = {
	{ 0x00, "call to be treated as a national call" },
	{ 0x01, "call to be treated as an international call" },
	{ 0, },
};

/* End-to-end method indicator */
static const struct key_val fwc_etem_vals[] = {
	{ 0x00, "no end-to-end method available (only link-by-link method available)" },
	{ 0x01, "pass-along method available (national use)" },
	{ 0x02, "SCCP method available" },
	{ 0x03, "pass-along and SCCP methods available (national use)" },
	{ 0, },
};

/* Interworking indicator */
static const struct key_val fwc_iw_vals[] = {
	{ 0x00, "no interworking encountered (No. 7 signalling all the way)" },
	{ 0x01, "interworking encountered" },
	{ 0, },
};

/* End-to-end information indicator */
static const struct key_val fwc_etei_vals[] = {
	{ 0x00, "no end-to-end information available" },
	{ 0x01, "end-to-end information available" },
	{ 0, },
};

/* ISDN user part indicator */
static const struct key_val fwc_isup_vals[] = {
	{ 0x00, "ISDN user part not used all the way" },
	{ 0x01, "ISDN user part used all the way" },
	{ 0, },
};

/* ISDN user part preference indicator */
static const struct key_val fwc_isup_pref_vals[] = {
	{ 0x00, "ISDN user part preferred all the way" },
	{ 0x01, "ISDN user part not required all the way" },
	{ 0x02, "ISDN user part required all the way" },
	{ 0x03, "spare" },
	{ 0, },
};

/* ISDN access indicator */
static const struct key_val fwc_ia_vals[] = {
	{ 0x00, "originating access non-ISDN" },
	{ 0x01, "originating access ISD" },
	{ 0, },
};

/* SCCP method indicator */
static const struct key_val fwc_sccpm_vals[] = {
	{ 0x00, "no indication" },
	{ 0x01, "connectionless method available (national use)" },
	{ 0x02, "connection oriented method available" },
	{ 0x03, "connectionless and connection oriented methods available (national use)" },
	{ 0, },
};

/* Transmission medium */
static const struct key_val trans_medium_vals[] = {
	{ 0x00, "speech" },
	{ 0x01, "spare" },
	{ 0x02, "64 kbit/s unrestricted" },
	{ 0x03, "3.1 kHz audio" },
	{ 0x04, "reserved for alternate speech (service 2)/64 kbit/s unrestricted (service 1)" },
	{ 0x05, "reserved for alternate 64 kbit/s unrestricted (service 1)/speech (service 2)" },
	{ 0x06, "64 kbit/s preferred" },
	{ 0x07, "2 × 64 kbit/s unrestricted" },
	{ 0x08, "384 kbit/s unrestricted" },
	{ 0x09, "1536 kbit/s unrestricted" },
	{ 0x0A, "1920 kbit/s unrestricted" },
	{ 0x10, "3 × 64 kbit/s unrestricted" },
	{ 0x11, "4 × 64 kbit/s unrestricted" },
	{ 0x12, "5 × 64 kbit/s unrestricted" },
	{ 0x13, "spare" },
	{ 0x14, "7 × 64 kbit/s unrestricted" },
	{ 0x15, "8 × 64 kbit/s unrestricted" },
	{ 0x16, "9 × 64 kbit/s unrestricted" },
	{ 0x17, "10 × 64 kbit/s unrestricted" },
	{ 0x18, "11 × 64 kbit/s unrestricted" },
	{ 0x19, "12 × 64 kbit/s unrestricted" },
	{ 0x1A, "13 × 64 kbit/s unrestricted" },
	{ 0x1B, "14 × 64 kbit/s unrestricted" },
	{ 0x1C, "15 × 64 kbit/s unrestricted" },
	{ 0x1D, "16 × 64 kbit/s unrestricted" },
	{ 0x1E, "17 × 64 kbit/s unrestricted" },
	{ 0x1F, "18 × 64 kbit/s unrestricted" },
	{ 0x20, "19 × 64 kbit/s unrestricted" },
	{ 0x21, "20 × 64 kbit/s unrestricted" },
	{ 0x22, "21 × 64 kbit/s unrestricted" },
	{ 0x23, "22 × 64 kbit/s unrestricted" },
	{ 0x24, "23 × 64 kbit/s unrestricted" },
	{ 0x25, "spare" },
	{ 0x26, "25 × 64 kbit/s unrestricted" },
	{ 0x27, "26 × 64 kbit/s unrestricted" },
	{ 0x28, "27 × 64 kbit/s unrestricted" },
	{ 0x29, "28 × 64 kbit/s unrestricted" },
	{ 0x2A, "29 × 64 kbit/s unrestricted" },
	{ 0, },
};

/* Q931 bearer capabilities */
static const struct key_val q931_cstd_vals[]  = {
	{ 0x00, "ITU-T standardized coding as described below" },
	{ 0x01, "ISO/IEC Standard" },
	{ 0x02, "National standard" },
	{ 0x03, "Standard defined for the network (either public or private) present on the network side of the interface" },
	{ 0, },
};

static const struct key_val q931_trs_cap_vals[] = {
	{ 0x00, "Speech" },
	{ 0x08, "Unrestricted digital information" },
	{ 0x09, "Restricted digital information" },
	{ 0x10, "3.1 kHz audio" },
	{ 0x11, "Unrestricted digital information with tones/announcements (Note 2)" },
	{ 0x18, "Video" },
	{ 0, },
};

static const struct key_val q931_trs_mde_vals[] = {
	{ 0x00, "Circuit mode" },
	{ 0x01, "Packet mode" },
	{ 0, },
};

static const struct key_val q931_trs_rte_vals[] = {
	{ 0x00, "packet-mode calls" },
	{ 0x10, "64 kbit/s" },
	{ 0x11, "2 × 64 kbit/s" },
	{ 0x13, "384 kbit/s" },
	{ 0x15, "1536 kbit/s" },
	{ 0x17, "1920 kbit/s" },
	{ 0x18, "Multirate (64 kbit/s base rate)" },
	{ 0, },
};

static const struct key_val q931_usr_info_vals[] = {
	{ 0x00, "ITU-T standardized rate adaption V.110, I.460 and X.30. This implies the presence of octet 5a and optionally octets 5b, 5c and 5d as defined below" },
	{ 0x02, "G.711 u-law" },
	{ 0x03, "G.711 a-law" },
	{ 0x04, "G.721 ADPCM" },
	{ 0x05, "H.221 and H.242" },
	{ 0x06, "H.223 and H.245" },
	{ 0x07, "Non-ITU-T standardized rate adaption" },
	{ 0x08, "V.120" },
	{ 0x09, "X.31" },
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

static void append_e164(srjson_doc_t *doc, const char *name, const uint8_t *data, uint8_t len, const uint8_t type)
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

	if (type == ISUPCalledPartyNumber) {
		uint8_t inn = data[1] >> 7;
		srjson_AddNumberToObject(doc, obj, "inn", inn);
		srjson_AddStringToObject(doc, obj, "inn_name", lookup(inn_vals, inn, "Unknown"));
	} else {
		uint8_t ni = data[1] >> 7;
		uint8_t ap = (data[1] & 0x0C) >> 2;
		uint8_t si = (data[1] & 0x03);

		srjson_AddNumberToObject(doc, obj, "ni", ni);
		srjson_AddStringToObject(doc, obj, "ni_name", lookup(ni_vals, ni, "Unknown"));

		srjson_AddNumberToObject(doc, obj, "restrict", ap);
		srjson_AddStringToObject(doc, obj, "restrict_name", lookup(restrict_vals, ap, "Unknown"));

		srjson_AddNumberToObject(doc, obj, "screened", si);
		srjson_AddStringToObject(doc, obj, "screened_name", lookup(screened_vals, si, "Unknown"));
	}

	srjson_AddNumberToObject(doc, obj, "ton", data[0] & 0x7F);
	srjson_AddStringToObject(doc, obj, "ton_name", lookup(nai_vals, data[0] & 0x7F, "Unknown"));
	srjson_AddNumberToObject(doc, obj, "npi", (data[1] >> 4) & 0x07);
	srjson_AddStringToObject(doc, obj, "npi_name", lookup(npi_vals, (data[1] >> 4) & 0x07, "Unknown"));

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

static void append_hop_counter(srjson_doc_t *doc, const char *name, const uint8_t *data, uint8_t len)
{
	uint8_t hop;

	if (len < 1) {
		LM_ERR("Not enough data for hop counter\n");
		return;
	}

	memcpy(&hop, data, 1);
	srjson_AddNumberToObject(doc, doc->root, name, hop);
}

static void append_calling_party_category(srjson_doc_t *doc, const uint8_t *data, uint8_t len)
{
	srjson_t *obj;
	uint8_t cat;

	if (len < 1) {
		LM_ERR("Not enough data for transport medium requirement\n");
		return;
	}

	obj = srjson_CreateObject(doc);
	if (!obj) {
		LM_ERR("Can not allocate json object for transport medium requirement\n");
		return;
	}

	memcpy(&cat, data, 1);
	srjson_AddNumberToObject(doc, obj, "num", cat);
	srjson_AddStringToObject(doc, obj, "name", lookup(calling_cat_vals, cat, "Unknown"));

	srjson_AddItemToObject(doc, doc->root, "calling_party", obj);
}

static void append_nci(srjson_doc_t *doc, const char *name, const uint8_t *data, uint8_t len)
{
	uint8_t sat, con, ech;
	srjson_t *obj;

	if (len != 1) {
		LM_ERR("Unpexected size(%u) for nature of connection indicators\n", len);
		return;
	}

	obj = srjson_CreateObject(doc);
	if (!obj) {
		LM_ERR("Can not allocate json object for %s\n", name);
		return;
	}

	sat = data[0] & 0x03;
	con = (data[0] & 0x0C) >> 2;
	ech = (data[0] & 0x10) >> 4;

	srjson_AddNumberToObject(doc, obj, "satellite", sat);
	srjson_AddStringToObject(doc, obj, "satellite_name", lookup(nci_sat_vals, sat, "Unknown"));
	srjson_AddNumberToObject(doc, obj, "continuity_check", con);
	srjson_AddStringToObject(doc, obj, "continuity_check_name", lookup(nci_con_vals, sat, "Unknown"));
	srjson_AddNumberToObject(doc, obj, "echo_device", ech);
	srjson_AddStringToObject(doc, obj, "echo_device_name", lookup(nci_echo_vals, ech, "Unknown"));

	srjson_AddItemToObject(doc, doc->root, name, obj);
}

static void append_forward_call(srjson_doc_t *doc, const char *name, const uint8_t *data, uint8_t len)
{
	uint16_t val;
	srjson_t *obj;
	size_t i;
	uint8_t off = 0;

	static const struct bit_masks {
		uint8_t num_bits;
		const struct key_val *vals;
		const char *name;
		const char *bit_names;
	} bits[] = {
		{ 1, fwc_nic_vals,		"national_international_call",	"A"  },
		{ 2, fwc_etem_vals,		"end_to_end_method",		"CB" },
		{ 1, fwc_iw_vals,		"interworking",			"D"  },
		{ 1, fwc_etei_vals,		"end_to_end_information",	"E"  },
		{ 1, fwc_isup_vals,		"isup",				"F"  },
		{ 2, fwc_isup_pref_vals,	"isup_preference",		"HG" },
		{ 1, fwc_ia_vals,		"isdn_access",			"I"  },
		{ 2, fwc_sccpm_vals,		"sccp_method",			"KJ" },
	};

	if (len != 2) {
		LM_ERR("Unpexected size(%u) for forward call indicators\n", len);
		return;
	}

	obj = srjson_CreateObject(doc);
	if (!obj) {
		LM_ERR("Can not allocate json object for %s\n", name);
		return;
	}

	memcpy(&val, data, sizeof(val));

	for (i = 0; i < (sizeof(bits)/sizeof(bits[0])); ++i) {
		char buf[128];
		const struct bit_masks *mask_info = &bits[i];
		uint8_t mask = 0, tmp;
		int b;

		/* build a mask */
		for (b = 0; b < mask_info->num_bits; ++b)
			mask = (mask << 1) | 0x01;

		snprintf(buf, sizeof(buf), "%s_name", mask_info->name);

		tmp = (val >> off) & mask;
		srjson_AddNumberToObject(doc, obj, mask_info->name, tmp);
		srjson_AddStringToObject(doc, obj, buf, lookup(mask_info->vals, tmp, mask_info->bit_names));
		off += mask_info->num_bits;
	}

	srjson_AddItemToObject(doc, doc->root, name, obj);
}

static void append_transmission_medium(srjson_doc_t *doc, const uint8_t *data, const uint8_t len)
{
	srjson_t *obj;

	if (len != 1) {
		LM_ERR("Unpexected size(%u)\n", len);
		return;
	}

	obj = srjson_CreateObject(doc);
	if (!obj) {
		LM_ERR("Can not allocate json object\n");
		return;
	}

	srjson_AddNumberToObject(doc, obj, "num", data[0]);
	srjson_AddStringToObject(doc, obj, "name", lookup(trans_medium_vals, data[0], "Unknown"));

	srjson_AddItemToObject(doc, doc->root, "transmission_medium", obj);
}

/*
 * Decode the Q.931 bearer capabilities now. At least three octets
 * but for some channels it will be four. Also only ITU caps will
 * be parsed.
 */
static void append_user_information(srjson_doc_t *doc, const uint8_t *data, const uint8_t len)
{
	srjson_t *obj;
	uint8_t coding_standard, transfer_capability;
	uint8_t transfer_mode, transfer_rate;
	uint8_t layer1_ident, layer1_protocol;
	uint8_t octet5;
	int rate_multiplier = -1;

	if (len < 3) {
		LM_ERR("Insufficient size(%u)\n", len);
		return;
	}

	coding_standard = (data[0] & 0x60) >> 5;
	transfer_capability = data[0] & 0x1F;
	transfer_mode = (data[1] & 0x60) >> 5;
	transfer_rate = data[1] & 0x1F;

	if (transfer_rate == 0x18) {
		if (len < 4) {
			LM_ERR("Insufficient size(%u) for multirate\n", len);
			return;
		}
		rate_multiplier = data[2] & 0x7F;
		octet5 = data[3];
	} else
		octet5 = data[2];

	layer1_ident = (octet5 & 0x60) >> 5;
	layer1_protocol = octet5 & 0x1F;

	obj = srjson_CreateObject(doc);
	if (!obj) {
		LM_ERR("Can not allocate json object\n");
		return;
	}

	/* numbers first and maybe names if it is ITU */
	srjson_AddStringToObject(doc, obj, "coding_standard_name", lookup(q931_cstd_vals, coding_standard, "Unknown"));
	srjson_AddNumberToObject(doc, obj, "coding_standard", coding_standard);
	srjson_AddNumberToObject(doc, obj, "transfer_capability", transfer_capability);

	srjson_AddNumberToObject(doc, obj, "transfer_mode", transfer_mode);
	srjson_AddNumberToObject(doc, obj, "transfer_rate", transfer_rate);
	if (rate_multiplier >= 0)
		srjson_AddNumberToObject(doc, obj, "rate_multiplier", rate_multiplier);
	srjson_AddNumberToObject(doc, obj, "layer1_ident", layer1_ident);
	srjson_AddNumberToObject(doc, obj, "layer1_protocol", layer1_protocol);


	/* ITU-T coding values */
	if (coding_standard == 0x00) {
		srjson_AddStringToObject(doc, obj, "transfer_capability_name",
						lookup(q931_trs_cap_vals, transfer_capability, "Unknown"));
		srjson_AddStringToObject(doc, obj, "transfer_mode_name",
						lookup(q931_trs_mde_vals, transfer_mode, "Unknown"));
		srjson_AddStringToObject(doc, obj, "transfer_rate_name",
						lookup(q931_trs_rte_vals, transfer_rate, "Unknown"));
		srjson_AddStringToObject(doc, obj, "layer1_protocol_name",
						lookup(q931_usr_info_vals, layer1_protocol, "Unknown"));
	}

	srjson_AddItemToObject(doc, doc->root, "user_information", obj);
}

static void isup_visitor(uint8_t type, const uint8_t *data, uint8_t len, struct isup_state *ptrs)
{
	switch (type) {
	case ISUPCalledPartyNumber:
		append_e164(ptrs->json, "called_number", data, len, ISUPCalledPartyNumber);
		break;
	case ISUPCallingPartyNumber:
		append_e164(ptrs->json, "calling_number", data, len, ISUPCallingPartyNumber);
		break;
	case ISUPCallingPartysCategory:
		append_calling_party_category(ptrs->json, data, len);
		break;
	case ISUPCauseIndicators:
		append_cause(ptrs->json, "cause", data, len);
		break;
	case ISUPEventInformation:
		append_event_information(ptrs->json, "event", data, len);
		break;
	case ISUPHopCounter:
		append_hop_counter(ptrs->json, "hop_counter", data, len);
		break;
	case ISUPNatureOfConnectionIndicators:
		append_nci(ptrs->json, "nature_of_connnection", data, len);
		break;
	case ISUPForwardCallIndicators:
		append_forward_call(ptrs->json, "forward_call", data, len);
		break;
	case ISUPTransmissionMediumRequirement:
		append_transmission_medium(ptrs->json, data, len);
		break;
	case ISUPUserServiceInformation:
		append_user_information(ptrs->json, data, len);
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
