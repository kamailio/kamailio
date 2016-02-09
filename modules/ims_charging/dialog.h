#ifndef __DIALOG_H
#define __DIALOG_H

#include "../../modules/ims_dialog/dlg_load.h"
#include "../../modules/ims_dialog/dlg_hash.h"
#include "../cdp/cdp_load.h"
#include "ims_ro.h"

extern int ro_timer_buffer;

void dlg_callback_received(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params);
void dlg_terminated(struct dlg_cell *dlg, int type, unsigned int termcode, char* reason, struct dlg_cb_params *_params);
void dlg_answered(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params);

#endif
