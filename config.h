/*
 *  $Id
 */



#ifndef config_h
#define config_h

/* default sip port if none specified */
#define SIP_PORT 5060

#define CFG_FILE "./sip_router.cfg"

/* receive buffer size */
#define BUF_SIZE 65507

/* maximum number of addresses on which we will listen */
#define MAX_LISTEN 16

/* default number of child processes started */
#define CHILD_NO    8

#endif
