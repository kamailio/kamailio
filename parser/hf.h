/* 
 * $Id$ 
 */

#ifndef HF_H
#define HF_H

#include "../str.h"


/* Header types and flags */
#define HDR_EOH                -1  /* End of header found */
#define HDR_ERROR               0  /* Error while parsing */
#define HDR_VIA                 1  /* Via header field */
#define HDR_VIA1                1  /* First Via header field */
#define HDR_VIA2                2  /* only used as flag*/
#define HDR_TO                  4  /* To header field */
#define HDR_FROM                8  /* From header field */
#define HDR_CSEQ               16  /* CSeq header field */
#define HDR_CALLID             32  /* Call-Id header field */
#define HDR_CONTACT            64  /* Contact header field */
#define HDR_MAXFORWARDS       128  /* MaxForwards header field */
#define HDR_ROUTE             256  /* Route header field */
#define HDR_RECORDROUTE       512  /* Record-Route header field */
#define HDR_CONTENTTYPE      1024  /* Content-Type header field */
#define HDR_CONTENTLENGTH    2048  /* Content-Length header field */
#define HDR_AUTHORIZATION    4096  /* Authorization header field */
#define HDR_EXPIRES          8192  /* Expires header field */
#define HDR_PROXYAUTH       16384  /* Proxy-Authorization header field */
#define HDR_WWWAUTH         32768  /* WWW-Authorization header field */
#define HDR_SUPPORTED       65536  /* Supported header field */
#define HDR_REQUIRE        131072  /* Require header field */
#define HDR_PROXYREQUIRE   262144  /* Proxy-Require header field */
#define HDR_UNSUPPORTED    524288  /* Unsupported header field */
#define HDR_ALLOW         1048576  /* Allow header field */
#define HDR_OTHER         2097152  /* Some other header field */


/* 
 * Format: name':' body 
 */
struct hdr_field {   
	int type;                /* Header field type */
	str name;                /* Header field name */
	str body;                /* Header field body */
	void* parsed;            /* Parsed data structures */
	struct hdr_field* next;  /* Next header field in the list */
};


/* frees a hdr_field structure,
 * WARNING: it frees only parsed (and not name.s, body.s)
 */
void clean_hdr_field(struct hdr_field* hf);


/* frees a hdr_field list,
 * WARNING: frees only ->parsed and ->next
 */
void free_hdr_field_lst(struct hdr_field* hf);


#endif
