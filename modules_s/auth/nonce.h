#ifndef NONCE_H
#define NONCE_H

#include "../../str.h"
#include <time.h>

#define NONCE_LEN (8+8+32)


/*
 * Calculate nonce value
 */
void calc_nonce(char* _nonce, int _expires, int _retry, str* _secret);


/*
 * Check nonce value received from UA
 */
int check_nonce(char* _nonce, str* _secret);


/*
 * Get expiry time from nonce string
 */
time_t get_nonce_expires(char* _nonce);


/*
 * Get retry counter from nonce string
 */
int get_nonce_retry(char* _nonce);


/*
 * Check if the nonce is stale
 */
int nonce_is_stale(char* _nonce);


#endif
