/*
 * SDP parser helpers
 *
 * Copyright (C) 2008-2009 SOMA Networks, INC.
 * Copyright (C) 2010 VoIP Embedded, Inc
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



#ifndef _SDP_HLPR_FUNCS_H
#define  _SDP_HLPR_FUNCS_H

#include "../../str.h"
#include "../msg_parser.h"
#include "sdp.h"

/**
 * Detect the mixed part delimiter.
 *
 * Example: "boundary1"
 * Content-Type: multipart/mixed; boundary="boundary1"
 */
int get_mixed_part_delimiter(str * body, str * mp_delimiter);

int extract_field(str* body, str* value, str field);
int extract_rtpmap(str *body, str *rtpmap_payload, str *rtpmap_encoding, str *rtpmap_clockrate, str *rtpmap_parmas);
int extract_fmtp( str *body, str *fmtp_payload, str *fmtp_string );
int extract_ptime(str *body, str *ptime);
int extract_sendrecv_mode(str *body, str *sendrecv_mode, int *is_on_hold);
int extract_mediaip(str *body, str *mediaip, int *pf, char *line);
int extract_media_attr(str *body, str *mediamedia, str *mediaport, str *mediatransport, str *mediapayload, int *is_rtp);
int extract_bwidth(str *body, str *bwtype, str *bwwitdth);
int extract_candidate(str *body, sdp_stream_cell_t *stream);

/* RFC3605 attributes */
int extract_rtcp(str *body, str *rtcp);

/* RFC4975 attributes */
int extract_accept_types(str *body, str *accept_types);
int extract_accept_wrapped_types(str *body, str *accept_wrapped_types);
int extract_max_size(str *body, str *max_size);
int extract_path(str *body, str *path);

char *find_sdp_line(char *p, char *plimit, char linechar);
char *find_next_sdp_line(char *p, char *plimit, char linechar, char *defptr);

char* get_sdp_hdr_field(char* , char* , struct hdr_field* );

char *find_sdp_line_delimiter(char *p, char *plimit, str delimiter);
char *find_next_sdp_line_delimiter(char *p, char *plimit, str delimiter, char *defptr); 
char * find_first_sdp_line(char* pstart, char* plimit, char linechar,
		 char* defptr);
#endif
