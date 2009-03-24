/*
 * $Id: sdp.h 4807 2008-09-02 15:00:48Z osas $
 *
 * SDP parser interface
 *
 * Copyright (C) 2008-2009 SOMA Networks, INC.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *  1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef SDP_H
#define SDP_H

#include "../msg_parser.h"

typedef struct sdp_payload_attr {
	struct sdp_payload_attr *next;
	int payload_num; /**< payload index inside stream */
	str rtp_payload;
	str rtp_enc;
	str rtp_clock;
	str rtp_params;
	str sendrecv_mode;
	str ptime;
} sdp_payload_attr_t;

typedef struct sdp_stream_cell {
	struct sdp_stream_cell *next;
	/* c=<network type> <address type> <connection address> */
	int pf;         /**< connection address family: AF_INET/AF_INET6 */
	str ip_addr;    /**< connection address */
	int stream_num; /**< stream index inside a session */
	/* m=<media> <port> <transport> <payloads> */
	str media;
	str port;
	str transport;
	str payloads;
	int payloads_num;                         /**< number of payloads inside a stream */
	/* b=<bwtype>:<bandwidth> */
	str bw_type;                              /**< alphanumeric modifier giving the meaning of the <bandwidth> figure:
							CT - conference total;
							AS - application specific */
	str bw_width;                            /**< The <bandwidth> is interpreted as kilobits per second by default */
	str path;                                 /**< RFC4975: path attribute */
	str max_size;                             /**< RFC4975: max-size attribute */
	str accept_types;                         /**< RFC4975: accept-types attribute */
	str accept_wrapped_types;                 /**< RFC4975: accept-wrapped-types attribute */
	struct sdp_payload_attr **p_payload_attr; /**< fast access pointers to payloads */
	struct sdp_payload_attr *payload_attr;
} sdp_stream_cell_t;

typedef struct sdp_session_cell {
	struct sdp_session_cell *next;
	int session_num;  /**< session index inside sdp */
	str cnt_disp;     /**< the Content-Disposition header (for Content-Type:multipart/mixed) */
	/* b=<bwtype>:<bandwidth> */
	str bw_type;      /**< alphanumeric modifier giving the meaning of the <bandwidth> figure:
				CT - conference total;
				AS - application specific */
	str bw_width;   /**< The <bandwidth> is interpreted as kilobits per second by default */
	int streams_num;  /**< number of streams inside a session */
	struct sdp_stream_cell*  streams;
} sdp_session_cell_t;

/**
 * Here we hold the head of the parsed sdp structure
 */
typedef struct sdp_info {
	int sessions_num;	/**< number of SDP sessions */
	struct sdp_session_cell *sessions;
} sdp_info_t;


/*
 * Parse SDP.
 */
int parse_sdp(struct sip_msg* _m);

/**
 * Get a session for the current sip msg based on position inside SDP.
 */
sdp_session_cell_t* get_sdp_session(struct sip_msg* _m, int session_num);
/**
 * Get a session for the given sdp based on position inside SDP.
 */
sdp_session_cell_t* get_sdp_session_sdp(struct sdp_info* sdp, int session_num);

/**
 * Get a stream for the current sip msg based on positions inside SDP.
 */
sdp_stream_cell_t* get_sdp_stream(struct sip_msg* _m, int session_num, int stream_num);
/**
 * Get a stream for the given sdp based on positions inside SDP.
 */
sdp_stream_cell_t* get_sdp_stream_sdp(struct sdp_info* sdp, int session_num, int stream_num);

/**
 * Get a payload from a stream based on payload.
 */
sdp_payload_attr_t* get_sdp_payload4payload(sdp_stream_cell_t *stream, str *rtp_payload);

/**
 * Get a payload from a stream based on position.
 */
sdp_payload_attr_t* get_sdp_payload4index(sdp_stream_cell_t *stream, int index);

/**
 * Free all memory associated with parsed structure.
 *
 * Note: this will free up the parsed sdp structure (form PKG_MEM).
 */
void free_sdp(sdp_info_t** _sdp);


/**
 * Print the content of the given sdp_info structure.
 *
 * Note: only for debug purposes.
 */
void print_sdp(sdp_info_t* sdp);
/**
 * Print the content of the given sdp_session structure.
 *
 * Note: only for debug purposes.
 */
void print_sdp_session(sdp_session_cell_t* sdp_session);
/**
 * Print the content of the given sdp_stream structure.
 *
 * Note: only for debug purposes.
 */
void print_sdp_stream(sdp_stream_cell_t *stream);


#endif /* SDP_H */
