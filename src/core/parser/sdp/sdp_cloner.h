/*
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


#ifndef SDP_CLONER_H
#define SDP_CLONER_H

#include "sdp.h"


/**
 * Clone the given sdp_session_cell structure.
 */
sdp_session_cell_t * clone_sdp_session_cell(sdp_session_cell_t *session);
/**
 * Free all memory associated with the cloned sdp_session structure.
 *
 * Note: this will free up the parsed sdp structure (form SHM_MEM).
 */
void free_cloned_sdp_session(sdp_session_cell_t *_session);

/**
 * Clone the given sdp_info structure.
 *
 * Note: all cloned structer will be in SHM_MEM.
 */
sdp_info_t* clone_sdp_info(struct sip_msg* _m);
/**
 * Free all memory associated with the cloned sdp_info structure.
 *
 * Note: this will free up the parsed sdp structure (form SHM_MEM).
 */
void free_cloned_sdp(sdp_info_t* sdp);

#endif /* SDP_H */
