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

#ifndef AUG_SYSDEP_H
#define AUG_SYSDEP_H

/*
**  As necessary, detect operating system, CPU, and compiler
**  combinations, and establish defines that describe the
**  characteristics and requirements for the combination.
**
**  As each special case is encountered elsewhere in the code,
**  a new define should be added here for each affected system.
**
**  Defines take names like:
**
**	AUG_HAS_xxxx	System has capability xxxx
**	AUG_NO_xxxx	System doesn't have capability xxxx
**	AUG_BAD_xxxx	System has xxxx, but it's broken
**
**  Every system gets AUG_CONFIGURATION so we can reject misconfigured
**  compiles.  This should be set to an os/cpu/compiler description.
*/
#undef AUG_CONFIGURATION

/*
**  This list should be maintained as the definitive list of capabilities.
**  Add each new define here with a description and then in each system
**  dependent section as appropriate.
**
**  Please stick to the "#undef" format -- the aug_sysdep.sh script
**  uses these to report configurations.
*/
#undef AUG_HAS_SELECT_H		/* Select macros in <sys/select.h> instead of
				   <sys/time.h> or <sys/time.h> */
#undef AUG_BAD_FD_SET		/* FD_SET et al are broken (HP-UX) */

#undef AUG_HAS_LP		/* SysV style "lp" and "lpstat" commands */
#undef AUG_HAS_LP_REQUEST	/* Has the /usr/spool/lp/request directory.
				   Probably only ever in HP-UX */
#undef AUG_HAS_LPR		/* BSD style "lpr" and "/etc/printcap" */
#undef AUG_NO_PUTENV		/* Use setenv() instead of putenv() */
#undef AUG_HAS_PREAD		/* Has pread() (combined seek/read) */

/* If neither AUG_HAS_RAND48 nor AUG_HAS_RANDOM, rand() will be used */
#undef AUG_HAS_RAND48		/* Has lrand48/srand48 calls */
#undef AUG_HAS_RANDOM		/* Has random/srandom calls */

#undef AUG_HAS_SINCOS		/* -libm has a fast sincos() implementation */
#undef AUG_NO_IOVEC		/* Some system may not have readv/writev */
#undef AUG_NO_TIMES		/* Some system may not have times(2) */

#undef AUG_HAS_PSAX		/* ps takes "-ax" arg to show all procs */
#undef AUG_NO_TZARG		/* get/settimeofday takes no timezone arg */

#undef AUG_NO_CRYPT_H		/* crypt(3) declared in unistd.h instead of
				   crypt.h. */

#undef AUG_NO_TERMIOS		/* System does not have the termios interface */

#undef AUG_NO_TERMIO_H		/* No termio.h, only termios.h used */

#undef AUG_NO_DB		/* System doesn't support UCB's db(3) */

#undef AUG_NO_GETPAGESIZE	/* System does not have getpagesize() */
#undef AUG_NO_PTHREADS		/* System does not have Posix Threads support */

/*
----------------------------------------------------------------------------
----- SGI Irix with sgi C --------------------------------------------------
----------------------------------------------------------------------------
*/

#if defined(sgi) || defined(__sgi) || defined(__sgi__)

#define AUG_HAS_LP
#define AUG_CONFIGURATION	"SGI Irix with sgi C"
#define AUG_HAS_RAND48

typedef unsigned int augUInt32;

#endif /* sgi */

/*
----------------------------------------------------------------------------
----- Sun Solaris 2.x on SPARC or x86, with SUNpro C or GCC ----------------
----------------------------------------------------------------------------
*/
#if defined(sun) || defined(__sun) || defined(__sun__)

#define AUG_HAS_LP
#define AUG_HAS_PREAD
#define AUG_HAS_RAND48

#if defined(i386) || defined(__i386)

#if defined(__GNUC__)
#define AUG_CONFIGURATION	"Sun Solaris x86 with GCC"
#else
#define AUG_CONFIGURATION	"Sun Solaris x86 with SUNpro C"
#endif

typedef unsigned int augUInt32;

#endif

#if defined(sparc) || defined(__sparc)
#if defined(__svr4__) || defined(__SVR4)

#if defined(__GNUC__)
#define AUG_CONFIGURATION	"Sun Solaris 2.x SPARC with GCC"
#else
#define AUG_CONFIGURATION	"Sun Solaris 2.x SPARC with SUNpro C"
#endif
#endif /* svr4 */

typedef unsigned int augUInt32;

#endif /* sparc */

#endif /* sun */

/*
----------------------------------------------------------------------------
----- Linux x86 with GCC ---------------------------------------------------
----------------------------------------------------------------------------
*/
#ifdef __linux

#define AUG_HAS_LPR
#define AUG_HAS_RANDOM		/* Actually has AUG_HAS_RAND48 too */
#define AUG_HAS_PSAX

/* AUG_DEBIAN supplied on cc command line where appropriate */
#ifndef AUG_DEBIAN
#define AUG_NO_CRYPT_H
#endif

#if __GNUC__ <= 2 && __GNUC_MINOR__ <= 7
/* Basically, assume this is a really of version of Linux -- ie "gomer" */
#define AUG_NO_PTHREADS
#endif

#if defined(__i386)

#if defined(__GNUC__)
#define AUG_CONFIGURATION	"Linux x86 with GCC"
#endif

typedef unsigned int augUInt32;

#endif /* i386 */
#endif /* linux */

/*
----------------------------------------------------------------------------
----- FreeBSD x86 with GCC -------------------------------------------------
----------------------------------------------------------------------------
*/
#ifdef __FreeBSD__

#define AUG_HAS_LPR
#define AUG_HAS_RANDOM
#define AUG_HAS_PSAX
#define AUG_HAS_PREAD
#define AUG_NO_CRYPT_H
#define AUG_NO_TERMIO_H
#define AUG_NO_DB

/*  FreeBSD lacks these error codes.  */
#define ENODATA	ENOBUFS
#define EPROTO	EPROTOTYPE
#define EUNATCH	ENOPROTOOPT

/*  FreeBSD lacks these termios codes.  */
#define TCGETS	TIOCGETA
#define TCSETS	TIOCSETA
#define TCGETA	TIOCGETA
#define TCSETA	TIOCSETA
#define TCSETSW	TIOCSETAW
#define TCFLSH	TIOCFLUSH
#define termio termios

#if defined(__i386)

#if defined(__GNUC__)
#define AUG_CONFIGURATION	"FreeBSD x86 with GCC"
#endif

typedef unsigned int augUInt32;

#endif /* i386 */
#endif /* freebsd */

/*
----------------------------------------------------------------------------
----- HP-UX pa-risc with HP C ----------------------------------------------
----------------------------------------------------------------------------
*/

#ifdef __hpux

#define AUG_BAD_FD_SET			/* Not even fixed in HP-UX 10.x */
#define AUG_HAS_LP
#define AUG_HAS_LP_REQUEST
#define AUG_HAS_RAND48
#define AUG_HAS_SINCOS

#if !defined(__GNUC__)
#define AUG_CONFIGURATION	"HP-UX pa-risc with HP C"
#endif

typedef unsigned int augUInt32;

#endif /* hpux */

/*
----------------------------------------------------------------------------
----- AIX Configuration with xlC -------------------------------------------
----------------------------------------------------------------------------
*/

#ifdef _AIX

#define AUG_HAS_LP
#define AUG_HAS_LP_REQUEST
#define AUG_HAS_SELECT_H
#define AUG_HAS_RAND48
#define AUG_NO_CRYPT_H

#if !defined(__GNUC__)
#define AUG_CONFIGURATION       "AIX Configuration with xlC"
#endif

typedef unsigned int augUInt32;

#endif /* _AIX */

/*
----------------------------------------------------------------------------
----- Sun IUS with GCC (formerly Interactive Unix) -----------------------
----------------------------------------------------------------------------
*/

/*
**  This is only sufficient to build a basic libaug.a so selected
**  utilities can be ported with relative ease.
**
**  None of the folio stuff builds (no unix domain sockets), so when
**  collecting a fresh copy of $AUG/libaug, run "rm -f *fol*".
*/
#ifndef _AIX /* xlC can't handle these expressions */
#if #system(svr3) && #cpu(i386)

#define AUG_HAS_LP
#define AUG_HAS_RAND48
#define AUG_NO_CRYPT_H
#define AUG_CONFIGURATION	"Sun IUS x86 with GCC"

typedef unsigned int augUInt32;

#include <sys/bsdtypes.h>

#endif /* IUS */
#endif /* ! _AIX */

/*
----------------------------------------------------------------------------
*/

#ifndef AUG_CONFIGURATION
error: os/cpu/compiler combination not configured in $Source$ $Revision$
#endif

#endif /* AUG_SYSDEP_H */
