/*
 * $Id$
 *
 * Registrar errno
 */

#ifndef RERRNO_H
#define RERRNO_H


typedef enum rerr {
	R_OK = 0,     /* Everything went OK */
	R_UL_DEL_R,   /* Usrloc record delete failed */
	R_UL_GET_R,   /* Usrloc record get failed */
	R_UL_NEW_R,   /* Usrloc new record failed */
	R_INV_CSEQ,   /* Invalid CSeq value */
	R_UL_INS_C,   /* Usrloc insert contact failed */
	R_UL_INS_R,   /* Usrloc insert record failed */
	R_UL_DEL_C,   /* Usrloc contact delete failed */
	R_UL_UPD_C,   /* Usrloc contact update failed */
	R_TO_USER,    /* No username part in To URI */
	R_INV_EXP,    /* Invalid expires parameter in contact */
	R_INV_Q,      /* Invalid q parameter in contact */
	R_PARSE,      /* Error while parsing message */
	R_TO_MISS,    /* Missing To header field */
	R_CID_MISS,   /* Missing Call-ID header field */
	R_CS_MISS,    /* Missing CSeq header field */
	R_PARSE_EXP,  /* Error while parsing Expires */
	R_PARSE_CONT, /* Error while parsing Contact */
	R_STAR_EXP,   /* star and expires != 0 */
	R_STAR_CONT,  /* star and more contacts */
	R_OOO,        /* Out-Of-Order request */
	R_RETRANS     /* Request is retransmission */
} rerr_t;


extern rerr_t rerrno;


#endif /* RERRNO_H */
