/*
 * $Id$
 *
 * Max-Forwards header field parser macros
 */

#ifndef CASE_MAX_H
#define CASE_MAX_H


#define ARDS_CASE                            \
        if (val == _ards_) {                 \
	        hdr->type = HDR_MAXFORWARDS; \
	        p += 4;                      \
		goto dc_end;                 \
	}


#define FORW_CASE                 \
        switch(val) {             \
        case _Forw_:              \
	        p += 4;           \
	        val = READ(p);    \
                ARDS_CASE;        \
                                  \
	        val = unify(val); \
		ARDS_CASE;        \
	        goto other;       \
        }                                             


#define Max_CASE       \
     p += 4;           \
     val = READ(p);    \
     FORW_CASE;        \
                       \
     val = unify(val); \
     FORW_CASE;        \
     goto other;       \


#endif /* CASE_MAX_H */
