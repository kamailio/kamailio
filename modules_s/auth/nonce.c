#include "nonce.h"
#include "calc.h"
#include <time.h>
#include "utils.h"
#include "../../dprint.h"
#include "auth_mod.h"  /* module parameters */
#include "../../md5global.h"
#include "../../md5.h"
#include <string.h>

/*
 * Nonce related functions
 */

/*
 * Calculate nonce value
 * Nonce value consists of time in seconds since 1.1 1970 and
 * secret phrase
 */
void calc_nonce(char* _nonce, int _expires, int _retry, str* _secret)
{
	MD5_CTX ctx;
	char bin[16];

	DBG("exipires: %d, retry: %d\n", _expires, _retry);

	MD5Init(&ctx);
	
	to_hex(_nonce, (char*)&_expires, 4);
	MD5Update(&ctx, _nonce, 8);

	to_hex(_nonce + 8, (char*)&_retry, 4);
	MD5Update(&ctx, _nonce + 8, 8);

	MD5Update(&ctx, _secret->s, _secret->len);
	MD5Final(bin, &ctx);
	CvtHex(bin, _nonce + 16);

	DBG("calc_nonce(): nonce=%s\n", _nonce);
}


int check_nonce(char* _nonce, str* _secret)
{
	int expires, retry;
	char non[NONCE_LEN];

	if (!_nonce) return 0;  /* Invalid nonce */

	expires = get_nonce_expires(_nonce);
	retry = get_nonce_retry(_nonce);

	DBG("expires: %d\n", expires);
	DBG("retry: %d\n", retry);

	calc_nonce(non, expires, retry, _secret);

	if (!memcmp(non, _nonce, NONCE_LEN)) return 1;

	return 0;
}


static inline int hex_to_int(char* _str)
{
	unsigned int i, res = 0;
#ifdef PARANOID
	if (!_str) {
		return 0;
	}
#endif
	for(i = 0; i < 8; i++) {
		res *= 16;
		if ((_str[i] >= '0') && (_str[i] <= '9'))      res += _str[i] - '0';
		else if ((_str[i] >= 'a') && (_str[i] <= 'f')) res += _str[i] - 'a' + 10;
		else if ((_str[i] >= 'A') && (_str[i] <= 'F')) res += _str[i] - 'A' + 10;
		else return 0;
	}
	return res;
}



/*
 * Get expiry time from nonce string
 */
time_t get_nonce_expires(char* _nonce)
{
	return (time_t)hex_to_int(_nonce);
}


/*
 * Get retry counter from nonce string
 */
int get_nonce_retry(char* _nonce)
{
	if (!_nonce) return 0;
	return hex_to_int(_nonce + 8);
}



int nonce_is_stale(char* _nonce) 
{
	if (!_nonce) return 0;

	if (get_nonce_expires(_nonce) < time(NULL)) {
		return 1;
	} else {
		return 0;
	}
}
