/*
 * $Id$
 *
 * Call-ID header field parser macros
 */

#ifndef CASE_CALL_H
#define CASE_CALL_H


#define ID_CASE                      \
     switch(val) {                   \
     case __ID1_:                    \
	     hdr->type = HDR_CALLID; \
	     hdr->name.len = 7;      \
	     *(p + 3) = '\0';        \
	     return (p + 4);         \
	                             \
     case __ID2_:                    \
	     hdr->type = HDR_CALLID; \
	     p += 4;                 \
	     goto dc_end;            \
     }


#define Call_CASE      \
     p += 4;           \
     val = READ(p);    \
     ID_CASE;          \
                       \
     val = unify(val); \
     ID_CASE;          \
                       \
     goto other;


#endif /* CASE_CALL_H */
