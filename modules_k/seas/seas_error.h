#include "../../error.h"


/** SER ERRORS ARE NEGATIVE, SEAS ERROR CODES ARE POSITIVE */

#define SE_CANCEL_MSG "500 SEAS cancel error"
#define SE_CANCEL_MSG_LEN (sizeof(SE_CANCEL_MSG)-1)
#define SE_CANCEL 1

#define SE_UAC_MSG "500 SEAS uac error"
#define SE_UAC_MSG_LEN (sizeof(SE_UAC_MSG)-1)
#define SE_UAC 2
