#ifndef __DIALOG_H
#define __DIALOG_H

#include "../../modules/dialog_ng/dlg_load.h"
#include "../../modules/dialog_ng/dlg_hash.h"
#include "../cdp/cdp_load.h"
#include "ims_ro.h"

extern int ro_timer_buffer;

void dlg_callback_received(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params);
void dlg_terminated(struct dlg_cell *dlg, int type, unsigned int termcode, char* reason, struct dlg_cb_params *_params);
void dlg_answered(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params);
void add_dlg_data_to_contact(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params);
void remove_dlg_data_from_contact(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params);

#endif
