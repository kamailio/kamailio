/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of the BinRPC Library (libbinrpc).
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */


/*
 * Packet header format:
 * (big endian where it applies)
 *      4b      4b        4b     2b   2b       <var>          <var>
 *  | MAGIC  | VERS  || FLAGS  | LL | CL || total_len ... || cookie ... |
 *  total_len = payload len (doesn't include the packet header)
 *  LL = total length len -1 (number of bytes on which total len is
 *        represented)
 *  CL = cookie length -1 (number of bytes on which the cookie is represented)
 *  E.g.: LL= 0 => total_len is represented on 1 byte (LL+1)
 *        CL= 3 => cookie is represneted on 4 bytes (CL+1)

 */

#ifndef __BRPC_CALL_H__
#define __BRPC_CALL_H__

#include "value.h"

/**
 * Current BINRPC protocol version.
 */
#define BINRPC_PROTO_VER	0x2
/**
 * Magic constant.
 */
#define BINRPC_PROTO_MAGIC	0x0
/**
 * Header preamble byte size.
 */
#define HDR_START_BYTES	(/*magic+version*/1 + /*flags+LL+CL*/1)

#define MIN_PKT_LEN	(HDR_START_BYTES +/*min total_len*/1 + /*min cookie_len*/1)



#ifdef _LIBBINRPC_BUILD

#define HDR_LENS_OFF	1 /* offset (in B) of lenghts byte */
#define HDR_PLLEN_OFF	2 /* offset (in B) of payload len bytes */
#define VER_BITS		4 /* number of bits to represent version */
#define PKT_FLAGS_OFF	4 /* offset (in b) of flags field */
#define PKT_LL_OFF		2 /* offset (in b) of LL field */
#define PKT_LL_MASK		0x3 /* mask to obtain LL (after shifting) */
#define PKT_CL_MASK		0x3 /* mask to obtain CL */

#endif /* _LIBBINRPC_BUILD */

/**
 * Types of BINRPC calls.
 */
typedef enum {
	BRPC_CALL_REPLY,
	BRPC_CALL_REQUEST,
	/* TODO: single notification type? (half call) */
} brpc_ctype_t;

typedef uint32_t brpc_id_t;

enum PKG_FLAGS {
	/**
	 * The packet is a request; if flag is cleared, it's a reply.
	 */
	BRPC_FLG_REQUEST	= 1 << 0,
	/**
	 * The packet is a reply for a broken request; broken: malformed, function
	 * not supported or with diffrent signature etc.
	 */
	BRPC_FLG_ERROR		= 1 << 1,
	/**
	 * Reserved values.
	 */
	BRPC_FLG_2			= 1 << 2, /* TODO: "last reply" (subscription call) */
	BRPC_FLG_3			= 1 << 3,
};

typedef struct {
	/**
	 * Type of call (request,reply).
	 */
	brpc_ctype_t type;
	/**
	 * The call is a reply signaling an error.
	 */
	bool error;
	/**
	 * The call is read only.
	 */
	bool locked;

	/**
	 * Call identification (translation of cookie).
	 */
	brpc_id_t id;
	/**
	 * Values that the call incorporates.
	 */
	struct {
		struct brpc_list_head list;
		size_t cnt; /* how many top level elements in list */
	} vals;

	/**
	 * Serialized form of the call.
	 * It is valid for both received and locally generated (after serializing)
	 */
	brpc_bin_t nbuf;

	uint8_t *pos; /* start of parsing */

} brpc_t;



/**
 * API calls.
 */

/**
 * Calculates the BINRPC packet lenght as seen on wire.
 * @param buff Read buffer.
 * @param len How much of buff can be read.
 * @return If enough info to tell the lenght had been read, returns the 
 * message's length; otherwise, returns the negative of how much more must be 
 * read, so that the lenght can be determined.
 * lenght of the BINRPC packet.
 */
ssize_t brpc_pkt_len(const uint8_t *buff, size_t len);

/**
 * Checks if (a) current packet is supported (version understood).
 * @param first_byte The first read byte of a packet.
 * @return True if packet is supported.
 */
bool brpc_pkg_supported(uint8_t first_byte);

/**
 * Return a new BINRPC request.
 * @param mname The name of the BINRPC call.
 * @return Reference to new request.
 */
brpc_t *brpc_req(brpc_str_t method, brpc_id_t id);

#define brpc_req_c(_meth_, _id) \
	({ \
		brpc_str_t meth; \
		meth.val = _meth_; \
		meth.len = strlen(_meth_) + /*0-term*/1; \
		brpc_req(meth, _id); \
	})

/**
 * Mark a BINRPC answer as failure reply.
 * "Failure" doesn't mean functional error, but BINRPC error: invalid
 * signature, inexistent method etc.
 * @param rpl The reply context reference.
 * @param code Numerical identification of the failure. Can be NULL.
 * @param reason Reason of failure to be replied with. Can be NULL.
 * @return Status of operation.
 */
bool brpc_fault(brpc_t *rpl, const brpc_int_t *_code, 
		const brpc_str_t *_reason);

/**
 * Return a new BINRPC reply context.
 * @param req Reference to the request which is replied.
 * @return Reference to new reply context.
 */
brpc_t *brpc_rpl(const brpc_t *req);

/**
 * Releases any resources allocated to a BINRPC context (but not the context!)
 * @param call BINRPC context
 */
void brpc_finish(brpc_t *call);

/** 
 * Prepare call for network dispatching.
 * If no label was assigned to a request, the current time + pid is used!
 * @param call The RPC context.
 * @return Buffer with context's serialized representation. The buffer is
 * managed by the library at context destruction (it is read only).
 */
const brpc_bin_t *brpc_serialize(brpc_t *call);

/**
 * Build a BINRPC context from a string.
 * @param buff Reference to string buffer. The buffer is attached to the
 * returned context and will be mem managed by it: must have been alloc'ed by 
 * same allocator used by the library.
 * @param len Lenght of buffer.
 * @return Reference to new context.
 * @see brpc_strd_wirepkt
 * @see brpc_deserialize
 */
brpc_t *brpc_raw(uint8_t *buff, size_t len);

/**
 * Build a BINRPC context from a static buffer string. The buffer is
 * duplicated, using library's allocator.
 * @see brpc_raw
 */
brpc_t *brpc_deserialize(uint8_t *buff, size_t len);

/**
 * Parse values in call.
 * @param call Reference to call context to read values from.
 * @return status of operation.
 */
bool brpc_unpack(brpc_t *call);

/**
 * Parses only the method name of the request.
 * @param req Reference to request context.
 * @return Status of operation.
 */
bool brpc_unpack_method(brpc_t *req);

/**
 * Add a new value to the call. The call can be either a request or a
 * response.
 * @param call The context where to add the value.
 * @param val The value to add to context.
 * @return void
 */
bool brpc_add_val(brpc_t *call, brpc_val_t *val);

/**
 * Fetches the index'th value in the message.
 * @param call The RPC context to get the value from.
 * @param index The index, starting at 0, of the value to be fetched
 * @return The BINRPC value, or NULL if not found (unavailale index).
 */
brpc_val_t *brpc_fetch_val(brpc_t *call, size_t index);

/**
 * Retrieve a BINRPC request's method name.
 * @param call BINRPC request.
 * @return NULL on error _or_ a reference to method's name.
 */
const brpc_str_t *brpc_method(brpc_t *req);
/**
 * Retrieve the code&reason for a call failure.
 * @param rpl BINRPC reply.
 * @param code Out-parameter: reference to code, or NULL if not present.
 * @param reason Out-parameter: reference to reason string, or NULL if not 
 * present.
 * @return Status of operation.
 */
bool brpc_fault_status(brpc_t *rpl, 
		brpc_int_t **code, brpc_str_t **reason);

/**
 * Assamble call given the values for it (parameters or returns).
 * @param call The call to add the arguments/replies to.
 * @param ftm Format descriptor for the arguments. Syntax:
 *  c : char * (can be used for BINRPC string type)
 *  d : int
 *  i : brpc_int_t *
 *  s : brpc_str_t *
 *  b : brpc_bin_t *
 *  I : brpc_val_t *, type INT
 *  S : brpc_val_t *, type STR
 *  B : brpc_val_t *, type BIN
 *  A : brpc_val_t *, type AVP
 *  M : brpc_val_t *, type MAP
 *  L : brpc_val_t *, type LIST
 *  <>: group as avp
 *  {}: group as map
 *  []: group as list
 *  ` ' : (space): ignored (allow more legible grouping).
 *  `:' : as space (good with AVPs)
 *  `,' : as space
 *  ! : void ** (the rest of arguments are grouped into an array)
 *  Note: the `cisb' specified values are cloned (mem managed by lib). The
 *  `ISBAML' are (deeply) cloned if locked, otherwise taken over by lib. Any
 *  pointer can be NULL.
 * @return Status of operation.
 */
bool brpc_asm(brpc_t *call, const char *fmt, ...);
/**
 * Disassamble the values from a call.
 * @param call The call to retrieve the arguments from.
 * @param fmt Forat descriptors of the arguments. Syntax:
 *  c: char **
 *  d: int **
 *  i : brpc_int_t **
 *  s : brpc_str_t **
 *  b : brpc_bin_t **
 *  I : brpc_val_t **, type INT
 *  S : brpc_val_t **, type STR
 *  B : brpc_val_t **, type BIN
 *  A : brpc_val_t **, type AVP
 *  M : brpc_val_t **, type MAP
 *  L : brpc_val_t **, type LIST
 *  <>: group as avp
 *  {}: group as map
 *  []: group as list
 *  . ignore one single entry on current level.
 *  * ignore the rest of entris on current level.
 *  ` ' : (space): ignored (allow more legible grouping).
 *  `:' : as space (good with AVPs)
 *  `,' : as sapce
 *  & : void ** (the rest of arguments are grouped into an array: each element
 *  in this array is dereferenced and assigned the reference to the actual
 *  value).
 *  ! : void ** (the rest of arguments are grouped into an array: each element
 *  in this array is assigned the reference to the actual value).
 *  Note: the values retrieved are mem managed by the library and freed when 
 *  the call is finished(). The returned references can be NULL.
 * @return Status of operation.
 */
bool brpc_dsm(brpc_t *call, const char *fmt, ...);

/**
 * Build the string representation of the call.
 * The requests's methods are skipped. Errornous replies do not get a
 * representation, also.
 * @param call BINRPC call to get the representation of.
 * @param len Out-paramter, holding string's lenght (but excluding the
 * 0-terminator!); can be NULL.
 * @return 0-terminated string representing the call, with significance:
 *  i : integer type
 *  s : string type
 *  b : binary type
 *  <>: avp grouping
 *  {}: map grouping
 *  []: list grouping
 *  The string must be mem managed by caller.
 */
char *brpc_repr(brpc_t *call, size_t *len);

/**
 * Checks if a BRPC call description is (syntactically) valid (well 
 * formatted).
 * Allowed chars, for strict checks: `c', `d', `i', `s', `b', `I', `S', `B', 
 * `[', `]', `<', `>', `{', `}'.
 * Allowed extra chars, for lax checks: `A', `M', `L', ` ', `.', `*'.
 * See brpc_(un)pack() for their significance.
 * @param repr Call representation string.
 * @param strict Do a strict check (oposed to lax).
 */
bool brpc_repr_check(const char *repr, bool strict);
/**
 * Count number of top level elements carried in message. For requests, the
 * method name is counted out.
 * @param msg Reference to BINRPC message
 * @return Count of values or negative, on error.
 */
ssize_t brpc_val_cnt(brpc_t *msg);

#define brpc_type(_brpc_)		((const brpc_ctype_t)(_brpc_)->type)
#define brpc_id(_brpc_)			((const brpc_id_t)(_brpc_)->id)
#define brpc_is_fault(_brpc_)	((const bool)(_brpc_)->error)
#define brpc_nbuf(_brpc_)		((const brpc_str_t *)&(_brpc_)->nbuf)

#endif /* __BRPC_CALL_H__ */
