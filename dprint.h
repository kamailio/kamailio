/*
 * $Id$
 */


#ifndef dprint_h
#define dprint_h



void dprint (char* format, ...);

#ifdef NO_DEBUG
	#define DPrint(fmt, args...)
#else
	#define DPrint(fmt,args...) dprint(fmt, ## args);
#endif


#endif /* ifndef dprint_h */
