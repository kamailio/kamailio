/*
 * $Id$
 *
 * Record-Route header field parser macros
 */

#ifndef CASE_RECO_H
#define CASE_RECO_H


#define OUTE_CASE                            \
        if (val == _oute_) {                 \
	        hdr->type = HDR_RECORDROUTE; \
		p += 4;                      \
		goto dc_end;                 \
	}                                    \


#define RD_R_CASE                 \
        switch(val) {             \
        case _rd_R_:              \
	        p += 4;           \
	        val = READ(p);    \
		OUTE_CASE;        \
                                  \
	        val = unify(val); \
		OUTE_CASE;        \
	        goto other;       \
        }


#define Reco_CASE         \
        p += 4;           \
        val = READ(p);    \
        RD_R_CASE;        \
                          \
        val = unify(val); \
        RD_R_CASE;        \
        goto other;


#endif /* CASE_RECO_H */

