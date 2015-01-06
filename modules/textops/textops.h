/* Copyright (C) 2008 Telecats BV
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 */

/*!
 * \file
 * \brief Module interface
 * \ingroup textops
 * Module: \ref textops
 */


#ifndef TEXTOPS_H_
#define TEXTOPS_H_
#include "../../mod_fix.h"

int search_f(struct sip_msg*, char*, char*);
int search_append_f(struct sip_msg*, char*, char*);
int remove_hf_f(struct sip_msg* msg, char* str_hf, char* foo);
int add_hf_helper(struct sip_msg* msg, str *str1, str *str2, gparam_p hfval, int mode, gparam_p hfanc);
int is_privacy_f(struct sip_msg *msg, char *privacy, char *str2 );

int fixup_regexp_none(void** param, int param_no);
int fixup_free_regexp_none(void** param, int param_no);
int fixup_privacy(void** param, int param_no);
#endif /*TEXTOPS_H_*/
