/*
 * $Id$
 *
 * Contact, Content-Type, Content-Length header field parser macros
 */

#ifndef CASE_CONT_H
#define CASE_CONT_H


#define TH_CASE                                        \
        switch(val) {                                  \
        case _th12_:                                   \
                hdr->type = HDR_CONTENTLENGTH;         \
                hdr->name.len = 14;                    \
                *(p + 3) = '\0';                       \
                return (p + 4);                        \
        }                                              \
                                                       \
        if ((*p == 't') || (*p == 'T')) {              \
                p++;                                   \
                if ((*p == 'h') || (*p == 'H')) {      \
                        hdr->type = HDR_CONTENTLENGTH; \
                        p++;                           \
                        goto dc_end;                   \
                }                                      \
        }


#define LENG_TYPE_CASE                       \
        switch(val) {                        \
        case _Leng_:                         \
                p += 4;                      \
                val = READ(p);               \
                TH_CASE;                     \
                goto other;                  \
                                             \
        case _Type_:                         \
                hdr->type = HDR_CONTENTTYPE; \
                p += 4;                      \
                goto dc_end;                 \
        }


#define ACT_ENT_CASE                     \
        switch(val) {                    \
        case _act1_:                     \
	        hdr->type = HDR_CONTACT; \
	        hdr->name.len = 7;       \
	        *(p + 3) = '\0';         \
	        return (p + 4);          \
	                                 \
        case _act2_:                     \
	        hdr->type = HDR_CONTACT; \
	        p += 4;                  \
	        goto dc_end;             \
                                         \
        case _ent__:                     \
                p += 4;                  \
                val = READ(p);           \
                LENG_TYPE_CASE;          \
                                         \
                val = unify(val);        \
                LENG_TYPE_CASE;          \
                goto other;              \
        }                         


#define Cont_CASE      \
     p += 4;           \
     val = READ(p);    \
     ACT_ENT_CASE;     \
                       \
     val = unify(val); \
     ACT_ENT_CASE;     \
     goto other;


#endif
