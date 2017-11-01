/*
 * hep related structure
 *
 * Copyright (C) 2011-14 Alexandr Dubovikov <alexandr.dubovikov@gmail.com>
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

#ifndef _hep_h
#define _hep_h

#include "../../core/endianness.h"
#include "../../core/events.h"

#ifdef __IS_BIG_ENDIAN
#define to_le(x) bswap32(x)
#else
#define to_le(x) (x)
#endif

#ifdef __OS_solaris
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
#define IPPROTO_IPIP IPPROTO_ENCAP /* Solaris IPIP protocol has name ENCAP */
#endif

extern int hep_capture_on;
extern int hep_offset;
extern char *authkey;
extern char *correlation_id;

/* int hep_msg_received(char * buf, unsigned int len, struct receive_info * ri);*/
int hep_msg_received(sr_event_param_t *evp);

/* new method for events */
int hepv3_message_parse(char *buf, unsigned int len, sip_msg_t *msg);
int hepv2_message_parse(char *buf, unsigned int len, sip_msg_t *msg);
int hepv3_get_chunk(struct sip_msg *msg, char *buf, unsigned int len,
		int req_chunk, pv_param_t *param, pv_value_t *res);


struct hep_hdr
{
	u_int8_t hp_v;		/* version */
	u_int8_t hp_l;		/* length */
	u_int8_t hp_f;		/* family */
	u_int8_t hp_p;		/* protocol */
	u_int16_t hp_sport; /* source port */
	u_int16_t hp_dport; /* destination port */
};

struct hep_iphdr
{
	struct in_addr hp_src;
	struct in_addr hp_dst; /* source and dest address */
};

struct hep_timehdr
{
	u_int32_t tv_sec;  /* seconds */
	u_int32_t tv_usec; /* useconds */
	u_int16_t captid;  /* Capture ID node */
};

struct hep_timeinfo
{
	u_int32_t tv_sec;  /* seconds */
	u_int32_t tv_usec; /* useconds */
	u_int32_t captid;  /* Capture ID node */
};

struct hep_ip6hdr
{
	struct in6_addr hp6_src; /* source address */
	struct in6_addr hp6_dst; /* destination address */
};

/* HEPv3 types */

struct hep_chunk
{
	u_int16_t vendor_id;
	u_int16_t type_id;
	u_int16_t length;
} __attribute__((packed));

typedef struct hep_chunk hep_chunk_t;

struct hep_chunk_uint8
{
	hep_chunk_t chunk;
	u_int8_t data;
} __attribute__((packed));

typedef struct hep_chunk_uint8 hep_chunk_uint8_t;

struct hep_chunk_uint16
{
	hep_chunk_t chunk;
	u_int16_t data;
} __attribute__((packed));

typedef struct hep_chunk_uint16 hep_chunk_uint16_t;

struct hep_chunk_uint32
{
	hep_chunk_t chunk;
	u_int32_t data;
} __attribute__((packed));

typedef struct hep_chunk_uint32 hep_chunk_uint32_t;

struct hep_chunk_str
{
	hep_chunk_t chunk;
	char *data;
} __attribute__((packed));

typedef struct hep_chunk_str hep_chunk_str_t;

struct hep_chunk_ip4
{
	hep_chunk_t chunk;
	struct in_addr data;
} __attribute__((packed));

typedef struct hep_chunk_ip4 hep_chunk_ip4_t;

struct hep_chunk_ip6
{
	hep_chunk_t chunk;
	struct in6_addr data;
} __attribute__((packed));

typedef struct hep_chunk_ip6 hep_chunk_ip6_t;

struct hep_chunk_payload
{
	hep_chunk_t chunk;
	char *data;
} __attribute__((packed));

typedef struct hep_chunk_payload hep_chunk_payload_t;


struct hep_ctrl
{
	char id[4];
	u_int16_t length;
} __attribute__((packed));

typedef struct hep_ctrl hep_ctrl_t;


/* Structure of HEP */

struct hep_generic_recv
{
	hep_ctrl_t *header;
	hep_chunk_uint8_t *ip_family;
	hep_chunk_uint8_t *ip_proto;
	hep_chunk_uint16_t *src_port;
	hep_chunk_uint16_t *dst_port;
	hep_chunk_uint32_t *time_sec;
	hep_chunk_uint32_t *time_usec;
	hep_chunk_ip4_t *hep_src_ip4;
	hep_chunk_ip4_t *hep_dst_ip4;
	hep_chunk_ip6_t *hep_src_ip6;
	hep_chunk_ip6_t *hep_dst_ip6;
	hep_chunk_uint8_t *proto_t;
	hep_chunk_uint32_t *capt_id;
	hep_chunk_uint16_t *keep_tm;
	hep_chunk_str_t *auth_key;
	hep_chunk_str_t *correlation_id;
	hep_chunk_t *payload_chunk;
} __attribute__((packed));

typedef struct hep_generic_recv hep_generic_recv_t;

#define HEP3_PACK_INIT(buf)         \
	union                           \
	{                               \
		hep_chunk_uint8_t chunk8;   \
		hep_chunk_uint16_t chunk16; \
		hep_chunk_uint32_t chunk32; \
		hep_chunk_t chunkpl;        \
		uint16_t len;               \
	} _tmpu;                        \
	char *_tmp_p = (buf);           \
	memcpy(_tmp_p, "HEP3", 4);      \
	_tmp_p += 4 + 2 /* skip length */;

#define HEP3_PACK_FINALIZE(buf, lenp)                                          \
	do {                                                                       \
		_tmpu.len = htons(_tmp_p - (char *)(buf));                             \
		memcpy((void *)(&(((hep_ctrl_t *)(buf))->length)), (void *)&_tmpu.len, \
				2);                                                            \
		*lenp = _tmp_p - (char *)(buf);                                        \
	} while(0)

#define _HEP3_PACK_CHUNK_GENERIC(type, tmpvar, vid, tid, val) \
	do {                                                      \
		(tmpvar).chunk.vendor_id = htons(vid);                \
		(tmpvar).chunk.type_id = htons(tid);                  \
		(tmpvar).chunk.length = htons(sizeof(type));          \
		(tmpvar).data = (val);                                \
		memcpy(_tmp_p, (void *)&(tmpvar), sizeof(type));      \
		_tmp_p += sizeof(type);                               \
	} while(0)

#define HEP3_PACK_CHUNK_UINT8(vid, tid, val) \
	_HEP3_PACK_CHUNK_GENERIC(hep_chunk_uint8_t, _tmpu.chunk8, vid, tid, val)
#define HEP3_PACK_CHUNK_UINT16(vid, tid, val) \
	_HEP3_PACK_CHUNK_GENERIC(                 \
			hep_chunk_uint16_t, _tmpu.chunk16, vid, tid, htons(val))
#define HEP3_PACK_CHUNK_UINT16_NBO(vid, tid, val) \
	_HEP3_PACK_CHUNK_GENERIC(hep_chunk_uint16_t, _tmpu.chunk16, vid, tid, (val))
#define HEP3_PACK_CHUNK_UINT32(vid, tid, val) \
	_HEP3_PACK_CHUNK_GENERIC(                 \
			hep_chunk_uint32_t, _tmpu.chunk32, vid, tid, htonl(val))
#define HEP3_PACK_CHUNK_UINT32_NBO(vid, tid, val) \
	_HEP3_PACK_CHUNK_GENERIC(hep_chunk_uint32_t, _tmpu.chunk32, vid, tid, (val))

#define HEP3_PACK_CHUNK_DATA(vid, tid, val, len)                     \
	do {                                                             \
		_tmpu.chunkpl.vendor_id = htons(vid);                        \
		_tmpu.chunkpl.type_id = htons(tid);                          \
		_tmpu.chunkpl.length = htons(sizeof(hep_chunk_t) + (len));   \
		memcpy(_tmp_p, (void *)&_tmpu.chunkpl, sizeof(hep_chunk_t)); \
		_tmp_p += sizeof(hep_chunk_t);                               \
		memcpy(_tmp_p, (void *)(val), len);                          \
		_tmp_p += len;                                               \
	} while(0)

#define HEP3_PACK_CHUNK_IP6(vid, tid, paddr) \
	HEP3_PACK_CHUNK_DATA(vid, tid, paddr, sizeof(struct in6_addr))

#endif
