#ifndef _LIBSMS_SMS_H
#define _LIBSMS_SMS_H

#include "sms_funcs.h"

#define MAX_MEM  0
#define USED_MEM 1


int putsms( struct sms_msg *sms_messg, struct modem *mdm);

int getsms( struct incame_sms *sms, struct modem *mdm, int sim);

int check_memory( struct modem *mdm, int flag);

void swapchars(char* string, int len);

#endif

