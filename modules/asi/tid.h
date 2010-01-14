/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of sip-router, a free SIP server.
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

#ifndef __ASI_TID_H__
#define __ASI_TID_H__

#include "ut.h"

inline static str *tid2str(unsigned int h_index, unsigned int h_label)
{
	static char buff[2 * INT2STR_MAX_LEN + /* `:' */1 + /* 0-term */1];
	static str tid_str = {buff, sizeof(buff)};
	char *cursor, *intstr;
	int num_len;

	cursor = buff;
	intstr = int2str(h_index, &num_len);
	memcpy(cursor, intstr, num_len); cursor += num_len;
	cursor[0] = ':'; cursor += 1;
	intstr = int2str(h_label, &num_len);
	memcpy(cursor, intstr, num_len); cursor += num_len;
	cursor[0] = 0; cursor += 1;

	tid_str.len = cursor - buff;
	return &tid_str;
}

#endif /* __ASI_TID_H__ */
