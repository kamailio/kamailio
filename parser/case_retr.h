/* 
 * Expires Header Field Name Parsing Macros
 *
 * Copyright (C) 2007 iptelorg GmbH
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

/*! \file 
 * \brief Parser ::  Expires Header Field Name Parsing Macros
 *
 * \ingroup parser
 */


#ifndef CASE_RETR_H
#define CASE_RETR_H

#include "../comp_defs.h"
#include "keys.h"

#define RETR_TER_CASE				\
	switch(LOWER_DWORD(val)){	\
		case _ter1_:				\
			hdr->type = HDR_RETRY_AFTER_T;	\
			hdr->name.len = 11;				\
			return (p + 4);					\
		\
		case _ter2_:                     \
			hdr->type = HDR_RETRY_AFTER_T;	\
			p += 4;							\
			goto dc_end;					\
	}



#define RETR_Y_AF_CASE				\
	if (LOWER_DWORD(val)==_y_af_){	\
		p+=4;						\
		val=READ(p);				\
		RETR_TER_CASE;				\
		goto other;					\
	}




#define retr_CASE		\
	p+=4;			\
	val=READ(p);	\
	RETR_Y_AF_CASE;	\
	goto other;


#endif /* CASE_RETR_H */
