/*
 * $Id$
 */


#include "parse_hname2.h"
#include "keys.h"
#include "../ut.h"  /* q_memchr */

/*
 * Size of hash table, this is magic value
 * that ensures, that there are no synonyms for
 * frequently used keys (frequently used keys are
 * 4-byte parts of message headers we recognize)
 * WARNING ! This value MUST be recalculated if you want
 * a new header to be recognized
 */
#define HASH_TABLE_SIZE 9349


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


static struct ht_entry hash_table[HASH_TABLE_SIZE];


/*
 * Pointer to the hash table
 */
/*
static struct ht_entry *hash_table;
*/


/*
 * Declarations
 */
static inline char* skip_ws     (char* p, unsigned int size);
void                init_htable (void);
static void         set_entry   (unsigned int key, unsigned int val);
static inline int   unify       (int key);


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


/*
 * Parser macros
 */
#include "case_via.h"      /* Via */
#include "case_from.h"     /* From */
#include "case_to.h"       /* To */
#include "case_cseq.h"     /* CSeq */
#include "case_call.h"     /* Call-ID */
#include "case_cont.h"     /* Contact, Content-Type, Content-Length */
#include "case_rout.h"     /* Route */
#include "case_max.h"      /* Max-Forwards */
#include "case_reco.h"     /* Record-Route */
#include "case_auth.h"     /* Authorization */
#include "case_expi.h"     /* Expires */
#include "case_prox.h"     /* Proxy-Authorization, Proxy-Require */
#include "case_allo.h"     /* Allow */
#include "case_unsu.h"     /* Unsupported */
#include "case_requ.h"     /* Require */
#include "case_supp.h"     /* Supported */
#include "case_www.h"      /* WWW-Authenticate */


#define READ(val) \
(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))


#define FIRST_QUATERNIONS       \
        case _Via1_: Via1_CASE; \
	case _From_: From_CASE; \
	case _To12_: To12_CASE; \
	case _CSeq_: CSeq_CASE; \
	case _Call_: Call_CASE; \
	case _Cont_: Cont_CASE; \
	case _Rout_: Rout_CASE; \
	case _Max__: Max_CASE;  \
	case _Reco_: Reco_CASE; \
	case _Via2_: Via2_CASE; \
	case _Auth_: Auth_CASE; \
	case _Expi_: Expi_CASE; \
	case _Prox_: Prox_CASE; \
	case _Allo_: Allo_CASE; \
	case _Unsu_: Unsu_CASE; \
	case _Requ_: Requ_CASE; \
	case _Supp_: Supp_CASE; \
        case _WWW__: WWW_CASE;


#define PARSE_COMPACT(id)          \
        switch(*(p + 1)) {         \
        case ' ':                  \
	        hdr->type = id;    \
	        p += 2;            \
	        goto dc_end;       \
	                           \
        case ':':                  \
	        hdr->type = id;    \
	        hdr->name.len = 1; \
	        *(p + 1) = '\0';   \
	        return (p + 2);    \
        }                            


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
	FIRST_QUATERNIONS;

	default:
		switch(*p) {
		case 'T':                           
		case 't':                           
			switch(*(p + 1)) {          
			case 'o':                   
			case 'O':                   
			case ' ':                   
				hdr->type = HDR_TO; 
				p += 2;             
				goto dc_end;        
				
			case ':':                   
				hdr->type = HDR_TO; 
				hdr->name.len = 1;  
				*(p + 1) = '\0'; 
				return (p + 2);     
			}                           
			break;

		case 'V':                            
		case 'v':                            
			PARSE_COMPACT(HDR_VIA);
			break;
			
		case 'F':
		case 'f':
			PARSE_COMPACT(HDR_FROM);
			break;
			
		case 'I':
		case 'i':
			PARSE_COMPACT(HDR_CALLID);
			break;

		case 'M':
		case 'm':
			PARSE_COMPACT(HDR_CONTACT);
			break;

		case 'L':
		case 'l':
			PARSE_COMPACT(HDR_CONTENTLENGTH);
			break;

		case 'C':
		case 'c':
			PARSE_COMPACT(HDR_CONTENTTYPE);
			break;

		case 'K':
		case 'k':
			PARSE_COMPACT(HDR_SUPPORTED);
                        break;
		}
		
		val = unify(val);
		switch(val) {
		FIRST_QUATERNIONS;
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
		hdr->name.s = 0;
		hdr->name.len = 0;
		return 0;
	} else {
		hdr->type = HDR_OTHER;
		*p = '\0';
		hdr->name.len = p - hdr->name.s;
		return (p + 1);
	}
}


/* Number of distinct keys */
#define NUM_KEYS  592

/* Number of distinct values */
#define NUM_VALS 49


/*
 * Create synonym-less (precalculated) hash table
 */
void init_hfname_parser(void)
{
	int i, j, k;

	     /* Hash table values */
	unsigned int init_val[NUM_VALS] = {
		_Allo_, _Auth_, _oriz_, _atio_, _Call_, __ID2_, __ID1_, _Cont_,
		_act2_, _act1_, _ent__, _Leng_, _th12_, _Type_, _CSeq_, _Expi_,
		_res2_, _res1_, _From_, _Max__, _Forw_, _ards_, _Prox_, _y_Au_,
		_thor_, _izat_, _ion2_, _ion1_, _y_Re_, _quir_, _Reco_, _rd_R_,
		_oute_, _Requ_, _ire2_, _ire1_, _Rout_, _Supp_, _orte_, _To12_,
		_Unsu_, _ppor_, _ted2_, _ted1_, _Via2_, _Via1_, _WWW__, _enti_,
		_cate_
	};

	     /* Number of keys associated to each value */
	unsigned int key_nums[NUM_VALS] = {
		16, 16, 16, 16, 16,  4,  4, 16, 
		 8,  8,  8, 16,  4, 16, 16, 16, 
		 8,  8, 16,  8, 16, 16, 16,  8, 
		16, 16,  8,  8,  8, 16, 16,  8, 
		16, 16,  8,  8, 16, 16, 16,  4, 
		16, 16,  8,  8,  8,  8,  8, 16,
                16
	};

	     /* Hash table keys */
	unsigned int init_key[NUM_KEYS] = {
		_allo_, _allO_, _alLo_, _alLO_, _aLlo_, _aLlO_, _aLLo_, _aLLO_, 
		_Allo_, _AllO_, _AlLo_, _AlLO_, _ALlo_, _ALlO_, _ALLo_, _ALLO_, 
		_auth_, _autH_, _auTh_, _auTH_, _aUth_, _aUtH_, _aUTh_, _aUTH_, 
		_Auth_, _AutH_, _AuTh_, _AuTH_, _AUth_, _AUtH_, _AUTh_, _AUTH_, 
		_oriz_, _oriZ_, _orIz_, _orIZ_, _oRiz_, _oRiZ_, _oRIz_, _oRIZ_, 
		_Oriz_, _OriZ_, _OrIz_, _OrIZ_, _ORiz_, _ORiZ_, _ORIz_, _ORIZ_, 
		_atio_, _atiO_, _atIo_, _atIO_, _aTio_, _aTiO_, _aTIo_, _aTIO_, 
		_Atio_, _AtiO_, _AtIo_, _AtIO_, _ATio_, _ATiO_, _ATIo_, _ATIO_, 
		_call_, _calL_, _caLl_, _caLL_, _cAll_, _cAlL_, _cALl_, _cALL_, 
		_Call_, _CalL_, _CaLl_, _CaLL_, _CAll_, _CAlL_, _CALl_, _CALL_, 
		__id2_, __iD2_, __Id2_, __ID2_, __id1_, __iD1_, __Id1_, __ID1_, 
		_cont_, _conT_, _coNt_, _coNT_, _cOnt_, _cOnT_, _cONt_, _cONT_, 
		_Cont_, _ConT_, _CoNt_, _CoNT_, _COnt_, _COnT_, _CONt_, _CONT_, 
		_act2_, _acT2_, _aCt2_, _aCT2_, _Act2_, _AcT2_, _ACt2_, _ACT2_, 
		_act1_, _acT1_, _aCt1_, _aCT1_, _Act1_, _AcT1_, _ACt1_, _ACT1_, 
		_ent__, _enT__, _eNt__, _eNT__, _Ent__, _EnT__, _ENt__, _ENT__, 
		_leng_, _lenG_, _leNg_, _leNG_, _lEng_, _lEnG_, _lENg_, _lENG_, 
		_Leng_, _LenG_, _LeNg_, _LeNG_, _LEng_, _LEnG_, _LENg_, _LENG_, 
		_th12_, _tH12_, _Th12_, _TH12_, _type_, _typE_, _tyPe_, _tyPE_, 
		_tYpe_, _tYpE_, _tYPe_, _tYPE_, _Type_, _TypE_, _TyPe_, _TyPE_, 
		_TYpe_, _TYpE_, _TYPe_, _TYPE_, _cseq_, _cseQ_, _csEq_, _csEQ_, 
		_cSeq_, _cSeQ_, _cSEq_, _cSEQ_, _Cseq_, _CseQ_, _CsEq_, _CsEQ_, 
		_CSeq_, _CSeQ_, _CSEq_, _CSEQ_, _expi_, _expI_, _exPi_, _exPI_, 
		_eXpi_, _eXpI_, _eXPi_, _eXPI_, _Expi_, _ExpI_, _ExPi_, _ExPI_, 
		_EXpi_, _EXpI_, _EXPi_, _EXPI_, _res2_, _reS2_, _rEs2_, _rES2_, 
		_Res2_, _ReS2_, _REs2_, _RES2_, _res1_, _reS1_, _rEs1_, _rES1_, 
		_Res1_, _ReS1_, _REs1_, _RES1_, _from_, _froM_, _frOm_, _frOM_, 
		_fRom_, _fRoM_, _fROm_, _fROM_, _From_, _FroM_, _FrOm_, _FrOM_, 
		_FRom_, _FRoM_, _FROm_, _FROM_, _max__, _maX__, _mAx__, _mAX__, 
		_Max__, _MaX__, _MAx__, _MAX__, _forw_, _forW_, _foRw_, _foRW_, 
		_fOrw_, _fOrW_, _fORw_, _fORW_, _Forw_, _ForW_, _FoRw_, _FoRW_, 
		_FOrw_, _FOrW_, _FORw_, _FORW_, _ards_, _ardS_, _arDs_, _arDS_, 
		_aRds_, _aRdS_, _aRDs_, _aRDS_, _Ards_, _ArdS_, _ArDs_, _ArDS_, 
		_ARds_, _ARdS_, _ARDs_, _ARDS_, _prox_, _proX_, _prOx_, _prOX_, 
		_pRox_, _pRoX_, _pROx_, _pROX_, _Prox_, _ProX_, _PrOx_, _PrOX_, 
		_PRox_, _PRoX_, _PROx_, _PROX_, _y_au_, _y_aU_, _y_Au_, _y_AU_, 
		_Y_au_, _Y_aU_, _Y_Au_, _Y_AU_, _thor_, _thoR_, _thOr_, _thOR_, 
		_tHor_, _tHoR_, _tHOr_, _tHOR_, _Thor_, _ThoR_, _ThOr_, _ThOR_, 
		_THor_, _THoR_, _THOr_, _THOR_, _izat_, _izaT_, _izAt_, _izAT_, 
		_iZat_, _iZaT_, _iZAt_, _iZAT_, _Izat_, _IzaT_, _IzAt_, _IzAT_, 
		_IZat_, _IZaT_, _IZAt_, _IZAT_, _ion2_, _ioN2_, _iOn2_, _iON2_, 
		_Ion2_, _IoN2_, _IOn2_, _ION2_, _ion1_, _ioN1_, _iOn1_, _iON1_, 
		_Ion1_, _IoN1_, _IOn1_, _ION1_, _y_re_, _y_rE_, _y_Re_, _y_RE_, 
		_Y_re_, _Y_rE_, _Y_Re_, _Y_RE_, _quir_, _quiR_, _quIr_, _quIR_, 
		_qUir_, _qUiR_, _qUIr_, _qUIR_, _Quir_, _QuiR_, _QuIr_, _QuIR_, 
		_QUir_, _QUiR_, _QUIr_, _QUIR_, _reco_, _recO_, _reCo_, _reCO_, 
		_rEco_, _rEcO_, _rECo_, _rECO_, _Reco_, _RecO_, _ReCo_, _ReCO_, 
		_REco_, _REcO_, _RECo_, _RECO_, _rd_r_, _rd_R_, _rD_r_, _rD_R_, 
		_Rd_r_, _Rd_R_, _RD_r_, _RD_R_, _oute_, _outE_, _ouTe_, _ouTE_, 
		_oUte_, _oUtE_, _oUTe_, _oUTE_, _Oute_, _OutE_, _OuTe_, _OuTE_, 
		_OUte_, _OUtE_, _OUTe_, _OUTE_, _requ_, _reqU_, _reQu_, _reQU_, 
		_rEqu_, _rEqU_, _rEQu_, _rEQU_, _Requ_, _ReqU_, _ReQu_, _ReQU_, 
		_REqu_, _REqU_, _REQu_, _REQU_, _ire2_, _irE2_, _iRe2_, _iRE2_, 
		_Ire2_, _IrE2_, _IRe2_, _IRE2_, _ire1_, _irE1_, _iRe1_, _iRE1_, 
		_Ire1_, _IrE1_, _IRe1_, _IRE1_, _rout_, _rouT_, _roUt_, _roUT_, 
		_rOut_, _rOuT_, _rOUt_, _rOUT_, _Rout_, _RouT_, _RoUt_, _RoUT_, 
		_ROut_, _ROuT_, _ROUt_, _ROUT_, _supp_, _supP_, _suPp_, _suPP_, 
		_sUpp_, _sUpP_, _sUPp_, _sUPP_, _Supp_, _SupP_, _SuPp_, _SuPP_, 
		_SUpp_, _SUpP_, _SUPp_, _SUPP_, _orte_, _ortE_, _orTe_, _orTE_, 
		_oRte_, _oRtE_, _oRTe_, _oRTE_, _Orte_, _OrtE_, _OrTe_, _OrTE_, 
		_ORte_, _ORtE_, _ORTe_, _ORTE_, _to12_, _tO12_, _To12_, _TO12_, 
		_unsu_, _unsU_, _unSu_, _unSU_, _uNsu_, _uNsU_, _uNSu_, _uNSU_, 
		_Unsu_, _UnsU_, _UnSu_, _UnSU_, _UNsu_, _UNsU_, _UNSu_, _UNSU_, 
		_ppor_, _ppoR_, _ppOr_, _ppOR_, _pPor_, _pPoR_, _pPOr_, _pPOR_, 
		_Ppor_, _PpoR_, _PpOr_, _PpOR_, _PPor_, _PPoR_, _PPOr_, _PPOR_, 
		_ted2_, _teD2_, _tEd2_, _tED2_, _Ted2_, _TeD2_, _TEd2_, _TED2_, 
		_ted1_, _teD1_, _tEd1_, _tED1_, _Ted1_, _TeD1_, _TEd1_, _TED1_, 
		_via2_, _viA2_, _vIa2_, _vIA2_, _Via2_, _ViA2_, _VIa2_, _VIA2_, 
		_via1_, _viA1_, _vIa1_, _vIA1_, _Via1_, _ViA1_, _VIa1_, _VIA1_, 
		_www__, _wwW__, _wWw__, _wWW__, _Www__, _WwW__, _WWw__, _WWW__, 
		_enti_, _entI_, _enTi_, _enTI_, _eNti_, _eNtI_, _eNTi_, _eNTI_, 
		_Enti_, _EntI_, _EnTi_, _EnTI_, _ENti_, _ENtI_, _ENTi_, _ENTI_, 
		_cate_, _catE_, _caTe_, _caTE_, _cAte_, _cAtE_, _cATe_, _cATE_, 
		_Cate_, _CatE_, _CaTe_, _CaTE_, _CAte_, _CAtE_, _CATe_, _CATE_
	}; 

	     /* Mark all elements as empty */
	for(i = 0; i < HASH_TABLE_SIZE; i++) {
		set_entry(HASH_EMPTY, HASH_EMPTY);
	}

	k = 0;

	for(i = 0; i < NUM_VALS; i++) {
		for(j = 0; j < key_nums[i]; j++) {
			set_entry(init_key[k++], init_val[i]);
		}
	}
}
