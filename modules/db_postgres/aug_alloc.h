/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 *
 * Copyright (C) 2003 August.Net Services, LLC
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
 *
 * ---
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 *
 */
/*
** ________________________________________________________________________
**
**
**                      $RCSfile$
**                     $Revision$
**
**             Last change $Date$
**           Last change $Author$
**                        $State$
**                       $Locker$
**
**               Original author: Andrew Fullford
**
**           Copyright (C) August Associates  1995
**
** ________________________________________________________________________
*/

/*  AM_TYPE: (INSTALL_INC)  */

#ifndef AUG_ALLOC_H
#define AUG_ALLOC_H

#include <stdlib.h>

typedef struct
{
	int estimated_overhead_per_alloc;	/* assumes malloc overhead
						   is 8 bytes.  This is
						   probably low */
	unsigned long alloc_ops;		/* Total allocs since epoch */
	unsigned long free_ops;			/* Total frees since epoch */
	unsigned long realloc_ops;		/* Total reallocs since epoch */
	unsigned long current_bytes_allocated;	/* Running allocation total */
} augAllocStats;

#define aug_alloc(s,p) aug_alloc_loc((s),(p),augDAB__FILE__,augDAB__LINE__)
#define aug_realloc(s,m) aug_realloc_loc((s),(m),augDAB__FILE__,augDAB__LINE__)
#define aug_strdup(s,p) aug_strdup_loc((s),(p),augDAB__FILE__,augDAB__LINE__)
#define aug_vecdup(v,p) aug_vecdup_loc((v),(p),augDAB__FILE__,augDAB__LINE__)
#define aug_free(m) aug_free_loc((m),augDAB__FILE__,augDAB__LINE__)
#define aug_foster(m,p) aug_foster_loc((m),(p),augDAB__FILE__,augDAB__LINE__)

typedef void augNoMemFunc(size_t size, char *func, char *file, int line);
extern augNoMemFunc *aug_set_nomem_func(augNoMemFunc *new_func);

extern void *aug_alloc_loc(size_t size, void *parent, char *file, int line);
extern void *aug_realloc_loc(size_t size, void *prev, char *file, int line);
extern char *aug_strdup_loc(char *str, void *parent, char *file, int line);
extern char **aug_vecdup_loc(char **vec, void *parent, char *file, int line);
extern void aug_free_loc(void *mem, char *file, int line);
extern void aug_foster_loc(void *mem, void *new_parent, char *file, int line);

extern augAllocStats *aug_alloc_stats(void);

#endif /* AUG_ALLOC_H */
