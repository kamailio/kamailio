/*
 * $Id$
 */

#include "auth.h"
#include "utils.h"
#include "defs.h"
#include <netinet/in.h>
#include <netdb.h>
#include "../../forward.h"
#include "../../dprint.h"
#include "../../udp_server.h"
#include "../../mem/shm_mem.h"
#include "../../data_lump_rpl.h"
#include "../../msg_translator.h"
#include "cred.h"
#include "db.h"
#include "calc.h"
#include "../../md5global.h"
#include "../../md5.h"

static char tag[32];
static char auth_hf[AUTH_HF_LEN];


extern db_con_t* db_handle;


void auth_init(void)
{
	str  src[5];
	
	     /*some fix string*/
	src[0].s="one two three four -> testing!";
	src[0].len=30;
	     /*some fix string*/
	src[1].s="Sip Express router - sex" ;
	src[1].len=24;
	     /*some fix string*/
	src[2].s="I love you men!!!! (Jiri's idea :))";
	src[2].len=35;
	     /*proxy's IP*/
	src[3].s=(char*)addresses;
	src[3].len=4;
	     /*proxy's port*/
	src[4].s=port_no_str;
	src[4].len=port_no_str_len;
	
	MDStringArray(tag, src, 5);
}


static void to_hex(unsigned char* _dst, unsigned char *_src, int _src_len)
{
	unsigned short i;
	unsigned char j;
    
	for (i = 0; i < _src_len; i++) {
		j = (_src[i] >> 4) & 0xf;
		if (j <= 9)
			_dst[i*2] = (j + '0');
		else
			_dst[i*2] = (j + 'a' - 10);
		j = _src[i] & 0xf;
		if (j <= 9)
			_dst[i*2+1] = (j + '0');
		else
			_dst[i*2+1] = (j + 'a' - 10);
	}
}


/*
 * Calculate nonce value
 * Nonce value consists of time in seconds since 1.1 1970 and
 * secret phrase
 */
static inline void calc_nonce(char* _realm, unsigned char* _nonce)
{
	MD5_CTX ctx;
	time_t t;
	unsigned char bin[16];
	HASHHEX hash;
	
	t = time(NULL) / 60;
	to_hex(_nonce, (unsigned char*)&t, 8);
	
	MD5Init(&ctx);
	MD5Update(&ctx, _nonce, 8);
	MD5Update(&ctx, ":", 1);
	MD5Update(&ctx, NONCE_SECRET, NONCE_SECRET_LEN);
	MD5Final(bin, &ctx);
	CvtHex(bin, _nonce + 8);
}


/*
 * Create Proxy-Authenticate header field
 */
static inline void build_proxy_auth_hf(char* _realm, char* _buf, int* _len)
{
	     /* 8 hex chars time value + 32 hex char MD5 + '\0' */
	char nonce[41];
	calc_nonce(_realm, nonce);
	nonce[40] = '\0';

	     /* Currently we support qop=auth only */

	     /*
	      * FIXME: We prefer not to use qop since some clients claim
	      *        to support that but they don't
	      */
	*_len = snprintf(_buf, AUTH_HF_LEN,
			 "Proxy-Authenticate: Digest realm=\"%s\", nonce=\"%s\",algorithm=MD5\r\n", 
			 _realm, nonce);
}


/*
 * Create a response with given code and reason phrase
 * Optionaly add new headers specified in _hdr
 */
static int send_resp(struct sip_msg* _m, int code, char* _reason, char* _hdr, int _hdr_len)
{
	char* buf;
	struct sockaddr_in to;
	struct lump_rpl* ptr;
	unsigned int len;

	to.sin_family = AF_INET;
	if (update_sock_struct_from_via(&to, _m->via1) == -1) {
		LOG(L_ERR, "send_resp(): Cannot lookup reply dst: %s\n",
		    _m->via1->host.s);
		return -1;
	}

	/* Add new headers if there are any */
	if (_hdr) {
		ptr = build_lump_rpl(_hdr, _hdr_len);
		add_lump_rpl(_m, ptr);
	}

	buf = build_res_buf_from_sip_req(code, _reason, tag, 32, _m ,&len);
	if (!buf) {
		LOG(L_ERR, "send_resp(): Build of response failed\n");
		return -1;
	}

	udp_send(buf, len, (struct sockaddr*)&to, sizeof(struct sockaddr_in));
	return 1;
}



/*
 * Challenge a user to send credentials
 */
int challenge(struct sip_msg* _msg, char* _realm, char* _str2)
{
	char* buf;
	unsigned int len;
	struct sockaddr_in to;
	struct lump_rpl* ptr;
	int auth_hf_len;

	printf("challenge(): Entering\n");

	build_proxy_auth_hf(_realm, auth_hf, &auth_hf_len);
	if (send_resp(_msg, 407, MESSAGE_407, auth_hf, auth_hf_len) == -1) {
		LOG(L_ERR, "challenge(): Error while sending response\n");
		return -1;
	}
	return 0;
}


static int find_auth_hf(struct sip_msg* _msg, char* _realm, cred_t* _c)
{
	struct hdr_field* ptr;
	int res;
#ifdef PARANOID
	if ((!_msg) || (!_realm) || (!_c)) {
		LOG(L_ERR, "find_auth_hf(): Invalid parameter value\n");
		return -1;
	}
#endif

	ptr = _msg->headers;
	while(ptr) {
		if (!strcasecmp(AUTH_RESPONSE, ptr->name.s)) {
			res = hf2cred(ptr, _c); 
			if (res == -1) {
				LOG(L_ERR, "find_auth_hf(): Error while parsing credentials\n");
				return -1;
			}
			     /* Malformed digest field */
			if (res == -2) {
				if (send_resp(_msg, 400, MESSAGE_400, NULL, 0) == -1) {
					LOG(L_ERR, "find_auth_hf(): Error while sending 400 reply\n");
				}
				return -1;
			}
			     /* We only support digest scheme */
			if (_c->scheme == SCHEME_DIGEST) {
				     /* Check the realm */
				if (!memcmp(_realm, _c->realm.s, _c->realm.len)) {
					return 0;
				}
			}
		}
		ptr = ptr->next;
	}
	return -1;
}


static int get_ha1(str* _user, char* _realm, char* _ha1)
{
	db_key_t keys[] = {"user", "realm"};
	db_val_t vals[] = {{DB_STRING, 0, {.string_val = _user->s}},
			   {DB_STRING, 0, {.string_val = _realm}}
	};
	db_key_t col[] = {"ha1"};
	db_res_t* res;

	const char* ha1;

#ifdef PARANOID
	if ((!_realm) || (!_user) || (!_ha1)) {
		LOG(L_ERR, "get_ha1(): Invalid parameter value\n");
		return -1;
	}
#endif
	
	db_use_table(db_handle, DB_TABLE);
	if (db_query(db_handle, keys, vals, col, 2, 1, NULL, &res) == FALSE) {
		LOG(L_ERR, "get_ha1(): Error while querying database\n");
		return -1;
	}

	if (RES_ROW_N(res) == 0) {
		printf("get_ha1(): no result\n");
		return -1;
	}
        ha1 = ROW_VALUES(RES_ROWS(res))[0].val.string_val;
	memcpy(_ha1, ha1, strlen(ha1) + 1);

	db_free_query(db_handle, res);
	return 0;
}



int check_cred(cred_t* _cred, str* _method, char* _ha1)
{
	HASHHEX resp;
	HASHHEX HA1;
	HASHHEX hent;
	char* qop;
	unsigned char nonce[33];


#ifdef PARANOID
	if ((!_cred) || (!_ha1) || (!_method)) {
		LOG(L_ERR, "check_cred(): Invalid parameter value\n");
		return -1;
	}
#endif
	
	     /* We always send algorithm=MD5 and qop=auth, so we can
	      * hardwire these values here, if th client uses a
	      * different algorithm and qop, it will simply not
	      * authorize him
	      */
	//	DigestCalcHA1("md5", _cred->username.s, _cred->realm.s, _a1, _cred->nonce.s, _cred->cnonce.s, HA1);

        switch(_cred->qop) {
	case QOP_AUTH:
		qop = "auth";
		break;
	case QOP_AUTH_INT:
		qop = "auth-int";
		break;
	default:
		qop = "";
		break;
	}
		
	printf("before calc\n");
	calc_nonce(_cred->realm.s, nonce);
	printf("after calc\n");
	DigestCalcResponse(_ha1, nonce, _cred->nonce_count.s, _cred->cnonce.s, qop, _method->s,
			   _cred->uri.s, hent, resp);
	printf("after digest\n");

	DBG("check_cred(): Our result = %s\n", resp);

	if (!memcmp(resp, _cred->response.s, 32)) {
		printf("check_cred(): Authorization is OK\n");
		return 1;
	} else {
		printf("check_cred(): Authorization failed\n");
		return -1;
	}
}



int authorize(struct sip_msg* _msg, char* _realm, char* str2)
{
	char ha1[256];
	cred_t cred;
	int res;
	struct hdr_field* auth_hf;
	     /* We will have to parse the whole message header
	      * since there may be multible authorization header
	      * fields
	      */
	init_cred(&cred);

	if (parse_headers(_msg, HDR_EOH) == -1) {
		LOG(L_ERR, "authorize(): Error while parsing message header\n");
		return -1;
	}

	     /* Finds the first occurence of authorization header for the
	      * given realm
	      */
	if (find_auth_hf(_msg, _realm, &cred) == -1) {
		DBG("authorize(): Proxy-Authorization HF not found or malformed\n");
		return -1;
	}
	DBG("authorize(): Proxy-Authorization HF with the given realm found\n");

	     /*
	      * FIXME: For debugging purposes only
	      */
	print_cred(&cred);

	printf("Before ge_ha1\n");
	if (get_ha1(&cred.username, _realm, ha1) == -1) {
		LOG(L_ERR, "authorize(): Error while getting A1 string for user %s\n", cred.username.s);
		return -1;
	}
	
	if (validate_cred(&cred) == -1) {
		if (send_resp(_msg, 400, MESSAGE_400, NULL, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
		}
		return -1;
	}
	
	printf("Before check_cred\n");
        res = check_cred(&cred, &_msg->first_line.u.request.method, ha1);
	if (res == -1) {
		LOG(L_ERR, "authorize(): Error while checking credentials\n");
		return -1;
	} else return res;
}
