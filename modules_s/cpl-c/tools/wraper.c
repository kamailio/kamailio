/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>



/**************************  VARIABLES ************************************/
int   debug = 9;
int   log_stderr = 1;
char  *mem_block = 0;




/**************************  MEMORY  ***********************************/
struct fm_block{};
struct qm_block{};

#ifdef DBG_F_MALLOC
void* fm_malloc(struct fm_block* bl,unsigned int size,char* file, char* func,
					unsigned int line)
{
	return malloc(size);
}
#else
void* fm_malloc(struct fm_block* bl, unsigned int size)
{
	return malloc(size);
}
#endif


#ifdef DBG_F_MALLOC
void  fm_free(struct fm_block *bl, void* p, char* file, char* func, 
				unsigned int line)
{
	free(p);
}
#else
void  fm_free(struct fm_block* bl, void* p)
{
	free(p);
}
#endif


void* qm_malloc(struct qm_block* qm, unsigned int size)
{
	return malloc(size);
}

void qm_free(struct qm_block* qm, void* p)
{
	free(p);
}


/****************************  DEBUG  *************************************/

void dprint(char * format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr,format,ap);
	fflush(stderr);
	va_end(ap);
}


