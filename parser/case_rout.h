/*
 * $Id$
 *
 * Route header field parser macros
 */

#ifndef CASE_ROUT_H
#define CASE_ROUT_H


#define Rout_CASE                   \
     p += 4;                        \
     switch(*p) {                   \
     case 'e':                      \
     case 'E':                      \
	     hdr->type = HDR_ROUTE; \
	     p++;                   \
	     goto dc_end;           \
                                    \
     default:                       \
	     goto other;            \
     }


#endif /* CASE_ROUT_H */
