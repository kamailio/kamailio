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

#ifndef AUG_DEBUG_H
#define AUG_DEBUG_H


#ifdef DEBUG

#define augDAB_DEBUG	augTRUE

#ifdef __FILE__
#define augDAB__FILE__	__FILE__
#else
#define augDAB__FILE__	""
#endif
#ifdef __LINE__
#define augDAB__LINE__	__LINE__
#else
#define augDAB__LINE__	0
#endif

#else

#define augDAB_DEBUG	augFALSE
#define augDAB__FILE__	""
#define augDAB__LINE__	0

#endif /* DEBUG */

/*
**  Debugging levels.
**
**  Each function is tagged with a debugging level.  The initial
**  state is ``don't know'' which will trigger a match, a relatively
**  slow operation.  The result of the match is recorded so future
**  accesses will occur at integer test instruction speed.
*/
#define DAB_UNKNOWN	-1		/* Need to perform match */
#define DAB_OFF		0		/* Matched, but debugs disabled */
#define DAB_TRACE	25		/* Level implied by DABTRACE() macro */
#define DAB_STD		50		/* Level implied by DAB() macro */
#define DAB_BULK	75		/* Level implied by DABBULK() macro */

#if augDAB_DEBUG == augTRUE

#define DABNAME(name)	static char *aug_dab_func=name,			\
				    aug_dab_file[]=augDAB__FILE__;	\
			static short aug_dab_level=DAB_UNKNOWN, aug_dab_dummy

#define DABLEVEL(lev)							\
	(aug_dab_enabled &&						\
	 ((aug_dab_level == DAB_UNKNOWN &&				\
	   aug_dab_match(aug_dab_func,aug_dab_file,			\
			 augDAB__LINE__,&aug_dab_level) >= (lev)) ||	\
	  aug_dab_level >= (lev)) &&					\
	 aug_dab_pushinfo(aug_dab_func,aug_dab_file,augDAB__LINE__))

#define DABL(lev)	aug_dab_dummy = DABLEVEL(lev) && aug_dab_fmt
#define DAB		DABL(DAB_STD)
#define DABTRACE	DABL(DAB_TRACE)
#define DABBULK		DABL(DAB_BULK)
#define DABTEXT		aug_dab_text
#define DABDUMP		aug_dab_dump

#define DABSET(line)	aug_dab_set(line)
#define DABRESET()	aug_dab_reset()
#define DABLOAD(file)	aug_dab_load(file)

extern augBool aug_dab_enabled;

#else

#define DABNAME(name)
#define DABLEVEL(lev)	(augFALSE)
#define DABL(lev)	(augFALSE) &&
#define DAB		DABL(0)
#define DABTRACE	DABL(0)
#define DABBULK		DABL(0)
#define DABTEXT		DABL(0)
#define DABDUMP		DABL(0)

#define DABSET(line)
#define DABRESET()
#define DABLOAD(file)

#endif /* augDAB_DEBUG */

extern int aug_dab_match(char *func, char *file, int line, short *plevel);
extern augBool aug_dab_pushinfo(char *func, char *file, int line);
extern augBool aug_dab_fmt(char *fmt, ...);
extern void aug_dab_text(char *text);
extern void aug_dab_dump(char *data, int size);
extern void aug_dab_reset(void);
extern augBool aug_dab_set(char *line);
extern void aug_dab_load(char *file);

#endif /* AUG_DEBUG_H */
