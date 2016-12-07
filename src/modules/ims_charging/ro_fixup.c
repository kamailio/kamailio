/* 
 * File:   ro_fixup.c
 * Author: Jason Penton
 *
 * Created on 06 April 2011, 9:52 PM
 */

#include "ro_fixup.h"
#include "../../mod_fix.h"

int ro_send_ccr_fixup(void** param, int param_no) {
    if (strlen((char*) *param) <= 0) {
        LM_ERR("empty parameter %d not allowed\n", param_no);
        return -1;
    }

    return fixup_var_int_12(param, 1);
}

