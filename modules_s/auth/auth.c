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


/*
 * FIXME
 * Calculate nonce value
 */
static inline int calc_nonce(char* _realm, char* _nonce)
{
	memcpy(_nonce, "12345678901234567890123456789012", 32);
	_nonce[32] = '\0';
	return 0;
}


/*
 * Create Proxy-Authenticate header field
 */
static inline int build_proxy_auth_hf(char* _realm, char* _buf, int* _len)
{
	char nonce[33];
#ifdef PARANOID
	if ((!_buf) || (!_len) || (!_realm)) {
		LOG(L_ERR, "build_proxy_auth_hf(): Invalid parameter value\n");
		return -1;
	}
#endif
	if (calc_nonce(_realm, nonce) == -1) {
		LOG(L_ERR, "build_proxy_auth_hf(): Error while calculating nonce value\n");
		return -1;
	}

	*_len = snprintf(_buf, AUTH_HF_LEN,
			 "Proxy-Authenticate: Digest realm=\"%s\", nonce=\"%s\",qop=\"auth\"\r\n", 
			 _realm, nonce);
	return 0;
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

	to.sin_family = AF_INET;
	if (update_sock_struct_from_via(&to, _msg->via1) == -1) {
		LOG(L_ERR, "challenge(): Cannot lookup reply dst: %s\n",
		    _msg->via1->host.s);
		return -1;
	}

	if (build_proxy_auth_hf(_realm, auth_hf, &auth_hf_len) == -1) {
		LOG(L_ERR, "challenge(): Can't build Proxy Authenticate HF\n");
		return -1;
	}

	ptr = build_lump_rpl(auth_hf, auth_hf_len);
	add_lump_rpl(_msg, ptr);

	buf = build_res_buf_from_sip_req(407, MESSAGE_407, tag, 32, _msg ,&len);
	if (!buf) {
		LOG(L_ERR, "challenge(): Build of response failed\n");
		return -1;
	}

	udp_send(buf, len, (struct sockaddr*)&to, sizeof(struct sockaddr_in));
	return 1;
}


static int check_realm(struct hdr_field* _hf, char* _realm)
{
#ifdef PARANOID
	if ((!_hf) || (!_realm)) {
		LOG(L_ERR, "check_realm(): Invalid parameter value\n");
		return -1;
	}
#endif
	
	return 0;
}


static int find_auth_hf(struct sip_msg* _msg, char* _realm, struct hdr_field** _hf)
{
	struct hdr_field* ptr;
#ifdef PARANOID
	if ((!_msg) || (!_realm) || (!_hf)) {
		LOG(L_ERR, "find_auth_hf(): Invalid parameter value\n");
		return -1;
	}
#endif

	ptr = _msg->headers;
	while(ptr) {
		if (!strcasecmp(AUTH_RESPONSE, ptr->name.s)) {
			if (check_realm(ptr, _realm) == 0) {
				*_hf = ptr;
				return 0;
			}
		}
		ptr = ptr->next;
	}
	return -1;
}


int get_a1(str* _user, char* _realm, char* _a1)
{
	db_key_t keys[] = {"user", "realm"};
	db_val_t vals[] = {{DB_STRING, 0, {.string_val = _user->s}},
			   {DB_STRING, 0, {.string_val = _realm}}
	};
	db_key_t col[] = {"a1"};
	db_res_t* res;

	const char* a1;

#ifdef PARANOID
	if ((!_realm) || (!_user) || (!_a1)) {
		LOG(L_ERR, "get_a1(): Invalid parameter value\n");
		return -1;
	}
#endif
	
	db_use_table(db_handle, DB_TABLE);
	if (db_query(db_handle, keys, vals, col, 2, 1, NULL, &res) == FALSE) {
		LOG(L_ERR, "get_a1(): Error while querying database\n");
		return -1;
	}

        a1 = ROW_VALUES(RES_ROWS(res))[0].val.string_val;
	memcpy(_a1, a1, strlen(a1) + 1);

	db_free_query(db_handle, res);
	return 0;
}



int check_cred(cred_t* _cred, str* _method, char* _a1)
{
	HASHHEX resp;
	HASHHEX HA1;
	HASHHEX hent;
#ifdef PARANOID
	if ((!_cred) || (!_a1) || (!_method)) {
		LOG(L_ERR, "check_cred(): Invalid parameter value\n");
		return -1;
	}
#endif
	DigestCalcHA1("MD5", _cred->username.s, _cred->realm.s, _a1, _cred->nonce.s, _cred->cnonce.s, HA1);
	DigestCalcResponse(HA1, _cred->nonce.s, _cred->nonce_count.s, _cred->cnonce.s, "auth", _method->s,
			   _cred->uri.s, hent, resp);

	printf("res = %s\n", resp);
	printf("a1 = %s\n", _a1);
	printf("met = %s\n", _method->s);

	if (!memcmp(resp, _cred->response.s, 32)) {
		printf("Authorization OK\n");
		return 1;
	} else {
		printf("Authorization failed\n");
		return 0;
	}
}



int authorize(struct sip_msg* _msg, char* _realm, char* str2)
{
	char a1[256];
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
	if (find_auth_hf(_msg, _realm, &auth_hf) == -1) {
		DBG("authorize(): Proxy-Authorization HF not found\n");
		return -1;
	}
	DBG("authorize(): Proxy-Authorization HF found\n");

	if (hf2cred(auth_hf, &cred)) {
		LOG(L_ERR, "authorize(): Error while parsing header\n");
		return -1;
	}

	print_cred(&cred);

	if (get_a1(&cred.username, _realm, a1) == -1) {
		LOG(L_ERR, "authorize(): Error while getting A1 string for user %s\n", cred.username.s);
		return -1;
	}

        res = check_cred(&cred, &_msg->first_line.u.request.method, a1);
	if (res == -1) {
		LOG(L_ERR, "authorize(): Error while checking credentials\n");
		return -1;
	} else return res;
}

