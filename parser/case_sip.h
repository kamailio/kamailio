#ifndef CASE_SIP_H
#define CASE_SIP_H

#define atch_CASE                            \
        switch(LOWER_DWORD(val)) {          \
        case _atch_:                        \
		DBG("end of SIP-If-Match\n"); \
                hdr->type = HDR_SIPIFMATCH; \
                p += 4;                     \
                goto dc_end;                \
        }


#define ifm_CASE				\
	switch(LOWER_DWORD(val)) {		\
	case _ifm_:				\
		DBG("middle of SIP-If-Match: yet=0x%04x\n",LOWER_DWORD(val)); \
		p += 4;				\
		val = READ(p);			\
		atch_CASE;			\
		goto other;			\
	}
		
#define sip_CASE          \
	DBG("beginning of SIP-If-Match: yet=0x%04x\n",LOWER_DWORD(val)); \
        p += 4;           \
        val = READ(p);    \
        ifm_CASE;         \
        goto other;

#endif /* CASE_SIP_H */
