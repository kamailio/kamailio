/*
 * $Id$
 *
 */

#ifndef _MSGBUILDER_H
#define _MSGBUILDER_H

#define CSEQ "CSeq: "
#define CSEQ_LEN 6
#define TO "To: "
#define TO_LEN 4
#define CALLID "Call-ID: "
#define CALLID_LEN 9
#define CONTENT_LENGTH "Content-Length: "
#define CONTENT_LENGTH_LEN 16
#define FROM "From: "
#define FROM_LEN 6
#define FROMTAG ";tag="
#define FROMTAG_LEN 5

#define UAC_CSEQNR "1"
#define UAC_CSEQNR_LEN 1

#define UAC_CSEQNR "1"
#define UAC_CSEQNR_LEN 1

/* convenience macros */
#define memapp(_d,_s,_len) \
	do{\
		memcpy((_d),(_s),(_len));\
		(_d) += (_len);\
	}while(0);

#define  append_mem_block(_d,_s,_len) \
	do{\
		memcpy((_d),(_s),(_len));\
		(_d) += (_len);\
	}while(0);

#ifdef _OBSO
#define append_str(_p,_str) \
	do{ \
		memcpy((_p), (_str).s, (_str).len); \
		(_p)+=(_str).len); \
	} while(0);
#endif

char *build_local(struct cell *Trans, unsigned int branch,
	unsigned int *len, char *method, int method_len, str *to);

char *build_uac_request(  str msg_type, str dst, str from,
	str headers, str body, int branch,
	struct cell *t, int *len);

int t_calc_branch(struct cell *t,
	int b, char *branch, int *branch_len);
int t_setbranch( struct cell *t, struct sip_msg *msg, int b );


#endif
