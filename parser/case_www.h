/*
 * $Id$
 *
 * WWW-Authenticate header field parser macros
 */

#ifndef CASE_WWW_H
#define CASE_WWW_H


#define CATE_CASE                        \
        switch(val) {                    \
        case _cate_:                     \
                hdr->type = HDR_WWWAUTH; \
                p += 4;                  \
	        goto dc_end;             \
        }


#define ENTI_CASE                 \
        switch(val) {             \
        case _enti_:              \
                p += 4;           \
                val = READ(p);    \
                CATE_CASE;        \
                                  \
                val = unify(val); \
                CATE_CASE;        \
                goto other;       \
} 


#define WWW_AUTH_CASE             \
        switch(val) {             \
        case _Auth_:              \
	        p += 4;           \
                val = READ(p);    \
                ENTI_CASE;        \
                                  \
                val = unify(val); \
                ENTI_CASE;        \
	        goto other;       \
        }


#define WWW_CASE          \
        p += 4;           \
        val = READ(p);    \
        WWW_AUTH_CASE;    \
                          \
        val = unify(val); \
                          \
        WWW_AUTH_CASE;    \
        goto other;


#endif
