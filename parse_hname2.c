/*
 * $Id$
 */

/* 
 * TODO: Optimize short variants of headers
 *       Test, Test, Test....
 *       Hardwire into ser core
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msg_parser.h"
#include "strs.h"
#include "dprint.h"

/*
 * Size of hash table, this is magic value
 * that ensures, that there are no synonyms for
 * frequently used keys (frequently used keys are
 * 4-byte parts of message headers we recognize)
 * WARNING ! This value MUST be recalculated if you want
 * a new header to be recognized
 */
#define HASH_TABLE_SIZE 1471




/*
 * Hash function
 */
#define HASH_FUNC(val) ((val) % HASH_TABLE_SIZE)


/*
 * This constants marks empty hash table element
 */
#define HASH_EMPTY 0x2d2d2d2d


/*
 * Hash table entry
 */
struct ht_entry {
	unsigned int key;
	unsigned int value;
};

/*
hash_table = (struct ht_entry*)malloc(HASH_TABLE_SIZE 
	* sizeof(struct ht_entry));
*/

static struct ht_entry hash_table[ HASH_TABLE_SIZE ];

/*
 * Pointer to the hash table
 */
/*
static struct ht_entry *hash_table;
*/


/*
 * Declarations
 */
static inline char* q_memchr    (char* p, int c, unsigned int size);
static inline char* skip_ws     (char* p, unsigned int size);
void         init_htable (void);
static void         set_entry   (unsigned int key, unsigned int val);
static inline int   unify       (int key);

/*
 * Fast replacement of standard memchr function
 */
static inline char* q_memchr(char* p, int c, unsigned int size)
{
	char* end;

	end=p+size;
	for(;p<end;p++){
		if (*p==(unsigned char)c) return p;
	}
	return 0;
}


/*
 * Skip all whitechars and return position of the first
 * non-white char
 */
static inline char* skip_ws(char* p, unsigned int size)
{
	char* end;
	
	end = p + size;
	for(; p < end; p++) {
		if ((*p != ' ') && (*p != '\t')) return p;
	}
	return p;
}
	

/*
 * Used to initialize hash table
 */
static void set_entry(unsigned int key, unsigned int val)
{
	hash_table[HASH_FUNC(key)].key = key;
	hash_table[HASH_FUNC(key)].value = val;
}


/*
 * cSeQ -> CSeq and so on...
 */ 
static inline int unify(int key)
{
	register struct ht_entry* en;

	en = &hash_table[HASH_FUNC(key)];
	if (en->key == key) {
		return en->value;
	} else {
		return key;
	}
}


#define Via1_CASE         \
     hdr->type = HDR_VIA; \
     hdr->name.len = 3;   \
     *(p + 3) = '\0';     \
     return p + 4        


#define From_CASE          \
     hdr->type = HDR_FROM; \
     p += 4;               \
     goto dc_end          
                                                             

#define To12_CASE        \
     hdr->type = HDR_TO; \
     hdr->name.len = 2;  \
     *(p + 2) = '\0';    \
     return (p + 3)


#define CSeq_CASE          \
     hdr->type = HDR_CSEQ; \
     p += 4;               \
     goto dc_end


#define Call_CASE                      \
     p += 4;                           \
     val = READ(p);                    \
     switch(val) {                     \
     case _ID1:                        \
	     hdr->type = HDR_CALLID;   \
	     hdr->name.len = 7;        \
	     *(p + 3) = '\0';          \
	     return (p + 4);           \
	                               \
     case _ID2:                        \
	     hdr->type = HDR_CALLID;   \
	     p += 4;                   \
	     goto dc_end;              \
     }                                 \
                                       \
     val = unify(val);                 \
     switch(val) {                     \
     case _ID1:                        \
	     hdr->type = HDR_CALLID;   \
	     hdr->name.len = 7;        \
	     *(p + 3) = '\0';          \
	     return (p + 4);           \
	                               \
     case _ID2:                        \
	     hdr->type = HDR_CALLID;   \
	     p += 4;                   \
	     goto dc_end;              \
                                       \
     default: goto other;              \
     }                                 \
     break


#define Cont_CASE                     \
     p += 4;                          \
     val = READ(p);                   \
     switch(val) {                    \
     case act1:                       \
	     hdr->type = HDR_CONTACT; \
	     hdr->name.len = 7;       \
	     *(p + 3) = '\0';         \
	     return (p + 4);          \
	                              \
     case act2:                       \
	     hdr->type = HDR_CONTACT; \
	     p += 4;                  \
	     goto dc_end;             \
     }                                \
                                      \
     val = unify(val);                \
     switch(val) {                    \
     case act1:                       \
	     hdr->type = HDR_CONTACT; \
	     hdr->name.len = 7;       \
	     *(p + 3) = '\0';         \
	     return (p + 4);          \
	                              \
     case act2:                       \
	     hdr->type = HDR_CONTACT; \
	     p += 4;                  \
	     goto dc_end;             \
                                      \
     default: goto other;             \
     }                                \
     break


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
     }                              \
     break


#define Max_CASE                                   \
     p += 4;                                       \
     val = READ(p);                                \
     switch(val) {                                 \
     case Forw:                                    \
	     p += 4;                               \
	     val = READ(p);                        \
	     if (val == ards) {                    \
		     hdr->type = HDR_MAXFORWARDS;  \
		     p += 4;                       \
		     goto dc_end;                  \
	     }                                     \
                                                   \
	     val = unify(val);                     \
	     if (val == ards) {                    \
		     hdr->type = HDR_MAXFORWARDS;  \
		     p += 4;                       \
		     goto dc_end;                  \
	     }                                     \
	     goto other;                           \
     }                                             \
                                                   \
     val = unify(val);                             \
     switch(val) {                                 \
     case Forw:                                    \
	     p += 4;                               \
	     val = READ(p);                        \
	     if (val == ards) {                    \
		     hdr->type = HDR_MAXFORWARDS;  \
		     p += 4;                       \
		     goto dc_end;                  \
	     }                                     \
                                                   \
	     val = unify(val);                     \
	     if (val == ards) {                    \
		     hdr->type = HDR_MAXFORWARDS;  \
		     p += 4;                       \
		     goto dc_end;                  \
	     }                                     \
     default: goto other;                          \
     }                                             \
                                                   \
     break


#define Reco_CASE                                 \
     p += 4;                                      \
     val = READ(p);                               \
     switch(val) {                                \
     case rd_R:                                   \
	     p += 4;                              \
	     val = READ(p);                       \
	     if (val == oute) {                   \
		     hdr->type = HDR_RECORDROUTE; \
		     p += 4;                      \
		     goto dc_end;                 \
	     }                                    \
                                                  \
	     val = unify(val);                    \
	     if (val == oute) {                   \
		     hdr->type = HDR_RECORDROUTE; \
		     p += 4;                      \
		     goto dc_end;                 \
	     }                                    \
	     goto other;                          \
     }                                            \
                                                  \
     val = unify(val);                            \
     switch(val) {                                \
     case rd_R:                                   \
	     p += 4;                              \
	     val = READ(p);                       \
	     if (val == oute) {                   \
		     hdr->type = HDR_RECORDROUTE; \
		     p += 4;                      \
		     goto dc_end;                 \
	     }                                    \
                                                  \
	     val = unify(val);                    \
	     if (val == oute) {                   \
		     hdr->type = HDR_RECORDROUTE; \
		     p += 4;                      \
		     goto dc_end;                 \
	     }                                    \
     default: goto other;                         \
     }                                            \
     break


#define Via2_CASE         \
     hdr->type = HDR_VIA; \
     p += 4;              \
     goto dc_end


#define READ(val) \
(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))

/*
 * Yet another parse_hname - Ultra Fast version :-)
 */
char* parse_hname2(char* begin, char* end, struct hdr_field* hdr)
{
	register char* p;
	register int val;

	p = begin;
	val = READ(p);
	hdr->name.s = begin;

	if ((end - begin) < 4) {
		hdr->type = HDR_ERROR;
		return begin;
	}

	switch(val) {
	case Via1: Via1_CASE;
	case From: From_CASE;
	case To12: To12_CASE;
	case CSeq: CSeq_CASE;
	case Call: Call_CASE;
	case Cont: Cont_CASE;
	case Rout: Rout_CASE;
	case Max_: Max_CASE;
	case Reco: Reco_CASE;
        case Via2: Via2_CASE;

	default:
		switch(*p) {
		case 'T':
		case 't':
			switch(*(p + 1)) {
			case 'o':
			case 'O':
			case ' ':   /* Short form */
				hdr->type = HDR_TO;
				p += 2;
				goto dc_end;

			case ':':
				hdr->type = HDR_TO;
				hdr->name.len = 1;
				*(p + 1) = '\0';
				return (p + 2);
			}

		case 'V':
		case 'v':
			switch(*(p + 1)) {
			case ' ':
				hdr->type = HDR_VIA;
				p += 2;
				goto dc_end;
				
			case ':':
				hdr->type = HDR_VIA;
				hdr->name.len = 1;
				*(p + 1) = '\0';
				return (p + 2);
			}
			break;

		case 'F':
		case 'f':
			switch(*(p + 1)) {
			case ' ':
				hdr->type = HDR_FROM;
				p += 2;
				goto dc_end;
				
			case ':':
				hdr->type = HDR_FROM;
				hdr->name.len = 1;
				*(p + 1)= '\0';
				return (p + 2);
			}
			break;

		case 'I':
		case 'i':
			switch(*(p + 1)) {
			case ' ':
				hdr->type = HDR_CALLID;
				p += 2;
				goto dc_end;
				
			case ':':
				hdr->type = HDR_CALLID;
				hdr->name.len = 1;
				*(p + 1) = '\0';
				return (p + 2);
			}
			break;

		case 'M':
		case 'm':
			switch(*(p + 1)) {
			case ' ':
				hdr->type = HDR_CONTACT;
				p += 2;
				goto dc_end;
				
			case ':':
				hdr->type = HDR_CONTACT;
				hdr->name.len = 1;
				*(p + 1) = '\0';
				return (p + 2);
			}
			break;
		}
		
		val = unify(val);
		switch(val) {
		case Via1: Via1_CASE;
		case From: From_CASE;
		case To12: To12_CASE;
		case CSeq: CSeq_CASE;                                                             
		case Call: Call_CASE;
		case Cont: Cont_CASE;
		case Rout: Rout_CASE;                                                             
		case Max_: Max_CASE;
		case Reco: Reco_CASE;
		case Via2: Via2_CASE;
		default: goto other;
		}
        }

	     /* Double colon hasn't been found yet */
 dc_end:
       	p = skip_ws(p, end - p);
	if (*p != ':') {   
	        goto other;
	} else {
		hdr->name.len = p - hdr->name.s;
		*p = '\0';
		return (p + 1);
	}

	     /* Unknown header type */
 other:    
	p = q_memchr(p, ':', end - p);
	if (!p) {        /* No double colon found, error.. */
		hdr->type = HDR_ERROR;
		hdr->name.s = NULL;
		hdr->name.len = 0;
		return NULL;
	} else {
		hdr->type = HDR_OTHER;
		*p = '\0';
		hdr->name.len = p - hdr->name.s;
		return (p + 1);
	}
}


void init_htable(void)
{
	int i;

	     /*
	      * Create hash table array
	      */

/*
	hash_table = (struct ht_entry*)malloc(HASH_TABLE_SIZE * sizeof(struct ht_entry));

*/
	     /*
	      * Mark all elements as empty
	      */
	for(i = 0; i < HASH_TABLE_SIZE; i++) {
		set_entry(HASH_EMPTY, HASH_EMPTY);
	}

        set_entry(via1, Via1);
	set_entry(viA1, Via1);
	set_entry(vIa1, Via1);
        set_entry(vIA1, Via1);
        set_entry(Via1, Via1);
        set_entry(ViA1, Via1);
	set_entry(VIa1, Via1);
	set_entry(VIA1, Via1);
	
        set_entry(via2, Via2);
	set_entry(viA2, Via2);
	set_entry(vIa2, Via2);
	set_entry(vIA2, Via2);
	set_entry(Via2, Via2);
	set_entry(ViA2, Via2);
	set_entry(VIa2, Via2);
	set_entry(VIA2, Via2);
	
	set_entry(from, From);
	set_entry(froM, From);
	set_entry(frOm, From);
	set_entry(frOM, From);
	set_entry(fRom, From);
	set_entry(fRoM, From);
	set_entry(fROm, From);
	set_entry(fROM, From);
	set_entry(From, From);
	set_entry(FroM, From);
	set_entry(FrOm, From);
	set_entry(FrOM, From);
	set_entry(FRom, From);
	set_entry(FRoM, From);
	set_entry(FROm, From);
	set_entry(FROM, From);
	
	set_entry(to12, To12);
	set_entry(tO12, To12);
	set_entry(To12, To12);
	set_entry(TO12, To12);

	set_entry(to21, To21);
	set_entry(tO21, To21);
	set_entry(To21, To21);
	set_entry(TO21, To21);

	set_entry(cseq, CSeq);
	set_entry(cseQ, CSeq);
	set_entry(csEq, CSeq);
	set_entry(csEQ, CSeq);
	set_entry(cSeq, CSeq);
	set_entry(cSeQ, CSeq);
	set_entry(cSEq, CSeq);
	set_entry(cSEQ, CSeq);
	set_entry(Cseq, CSeq);
	set_entry(CseQ, CSeq);
	set_entry(CsEq, CSeq);
	set_entry(CsEQ, CSeq);
	set_entry(CSeq, CSeq);
	set_entry(CSeQ, CSeq);
	set_entry(CSEq, CSeq);
	set_entry(CSEQ, CSeq);
	
	set_entry(call, Call);
	set_entry(calL, Call);
	set_entry(caLl, Call);
	set_entry(caLL, Call);
	set_entry(cAll, Call);
	set_entry(cAlL, Call);
	set_entry(cALl, Call);
	set_entry(cALL, Call);
	set_entry(Call, Call);
	set_entry(CalL, Call);
	set_entry(CaLl, Call);
	set_entry(CaLL, Call);
	set_entry(CAll, Call);
	set_entry(CAlL, Call);
	set_entry(CALl, Call);
	set_entry(CALL, Call);

	set_entry(_id1, _ID1);
	set_entry(_iD1, _ID1);
	set_entry(_Id1, _ID1);
	set_entry(_ID1, _ID1);

	set_entry(_id2, _ID2);
	set_entry(_iD2, _ID2);
	set_entry(_Id2, _ID2);
	set_entry(_ID2, _ID2);

	set_entry(cont, Cont);
	set_entry(conT, Cont);
	set_entry(coNt, Cont);
	set_entry(coNT, Cont);
	set_entry(cOnt, Cont);
	set_entry(cOnT, Cont);
	set_entry(cONt, Cont);
	set_entry(cONT, Cont);
	set_entry(Cont, Cont);
	set_entry(ConT, Cont);
	set_entry(CoNt, Cont);
	set_entry(CoNT, Cont);
	set_entry(COnt, Cont);
	set_entry(COnT, Cont);
	set_entry(CONt, Cont);
	set_entry(CONT, Cont);

	set_entry(act1, act1);
	set_entry(acT1, act1);
	set_entry(aCt1, act1);
	set_entry(aCT1, act1);
	set_entry(Act1, act1);
	set_entry(AcT1, act1);
	set_entry(ACt1, act1);
	set_entry(ACT1, act1);

	set_entry(act2, act2);
	set_entry(acT2, act2);
	set_entry(aCt2, act2);
	set_entry(aCT2, act2);
	set_entry(Act2, act2);
	set_entry(AcT2, act2);
	set_entry(ACt2, act2);
	set_entry(ACT2, act2);

	set_entry(max_, Max_);
	set_entry(maX_, Max_);
	set_entry(mAx_, Max_);
	set_entry(mAX_, Max_);
	set_entry(Max_, Max_);
	set_entry(MaX_, Max_);
	set_entry(MAx_, Max_);
	set_entry(MAX_, Max_);

	set_entry(forw, Forw);
	set_entry(forW, Forw);
	set_entry(foRw, Forw);
	set_entry(foRW, Forw);
	set_entry(fOrw, Forw);
	set_entry(fOrW, Forw);
	set_entry(fORw, Forw);
	set_entry(fORW, Forw);
	set_entry(Forw, Forw);
	set_entry(ForW, Forw);
	set_entry(FoRw, Forw);
	set_entry(FoRW, Forw);
	set_entry(FOrw, Forw);
	set_entry(FOrW, Forw);
	set_entry(FORw, Forw);
	set_entry(FORW, Forw);

	set_entry(ards, ards);
	set_entry(ardS, ards);
	set_entry(arDs, ards);
	set_entry(arDS, ards);
	set_entry(aRds, ards);
	set_entry(aRdS, ards);
	set_entry(aRDs, ards);
	set_entry(aRDS, ards);
	set_entry(Ards, ards);
	set_entry(ArdS, ards);
	set_entry(ArDs, ards);
	set_entry(ArDS, ards);
	set_entry(ARds, ards);
	set_entry(ARdS, ards);
	set_entry(ARDs, ards);
	set_entry(ARDS, ards);

	set_entry(rout, Rout);
	set_entry(rouT, Rout);
	set_entry(roUt, Rout);
	set_entry(roUT, Rout);
	set_entry(rOut, Rout);
	set_entry(rOuT, Rout);
	set_entry(rOUt, Rout);
	set_entry(rOUT, Rout);
	set_entry(Rout, Rout);
	set_entry(RouT, Rout);
	set_entry(RoUt, Rout);
	set_entry(RoUT, Rout);
	set_entry(ROut, Rout);
	set_entry(ROuT, Rout);
	set_entry(ROUt, Rout);
	set_entry(ROUT, Rout);

	set_entry(reco, Reco);
	set_entry(recO, Reco);
	set_entry(reCo, Reco);
	set_entry(reCO, Reco);
	set_entry(rEco, Reco);
	set_entry(rEcO, Reco);
	set_entry(rECo, Reco);
	set_entry(rECO, Reco);
	set_entry(Reco, Reco);
	set_entry(RecO, Reco);
	set_entry(ReCo, Reco);
	set_entry(ReCO, Reco);
	set_entry(REco, Reco);
	set_entry(REcO, Reco);
	set_entry(RECo, Reco);
	set_entry(RECO, Reco);

	set_entry(rd_r, rd_R);
	set_entry(rd_R, rd_R);
	set_entry(rD_r, rd_R);
	set_entry(rD_R, rd_R);
	set_entry(Rd_r, rd_R);
	set_entry(Rd_R, rd_R);
	set_entry(RD_r, rd_R);
	set_entry(RD_R, rd_R);

	set_entry(oute, oute);
	set_entry(outE, oute);
	set_entry(ouTe, oute);
	set_entry(ouTE, oute);
	set_entry(oUte, oute);
	set_entry(oUtE, oute);
	set_entry(oUTe, oute);
	set_entry(oUTE, oute);
	set_entry(Oute, oute);
	set_entry(OutE, oute);
	set_entry(OuTe, oute);
	set_entry(OuTE, oute);
	set_entry(OUte, oute);
	set_entry(OUtE, oute);
	set_entry(OUTe, oute);
	set_entry(OUTE, oute);
}


