/*
 * $Id$
 *
 * Nonce related functions
 */

#ifndef NONCE_H
#define NONCE_H

#include "../../str.h"
#include <time.h>


/*
 * Length of nonce string in bytes
 */
#define NONCE_LEN (8+8+32)


/*
 * Calculate nonce value
 */
void calc_nonce(char* _nonce, int _expires, int _retry, str* _secret);


/*
 * Check nonce value received from UA
 */
int check_nonce(str* _nonce, str* _secret);


/*
 * Get expiry time from nonce string
 */
time_t get_nonce_expires(str* _nonce);


/*
 * Get retry counter from nonce string
 */
int get_nonce_retry(str* _nonce);


/*
 * Check if the nonce is stale
 */
int nonce_is_stale(str* _nonce);


#endif /* NONCE_H */
