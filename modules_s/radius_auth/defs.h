/* 
 * $Id$ 
 *
 * Common definitions
 */

#ifndef DEFS_H
#define DEFS_H

#define PARANOID

/*
 * Helper definitions
 */

/*
 * the module will accept and authorize also username
 * of form user@domain which some broken clients send
 */
#define USER_DOMAIN_HACK


/*
 * If the method is ACK, it is always authorized
 */
#define ACK_CANCEL_HACK


/* 
 * Send algorithm=MD5 in challenge
 */
#define PRINT_MD5


#endif /* DEFS_H */
