#include "dlg_mod_internal.h"
#include "dlg_handler.h"
#include "dlg_avps.h"
#include "dlg_storage.h"

static int get_dialog_id(struct sip_msg *m, dlg_id_t *id)
{
	if (parse_headers(m, HDR_FROM_F | HDR_TO_F | HDR_CALLID_F, 0) < 0) {
		ERR("can't parse headers\n");
		return -1;
	}
	
	memset(id, 0, sizeof(*id));
	if (m->to) id->loc_tag = ((struct to_body*)m->to->parsed)->tag_value;
	if (m->from) id->rem_tag = ((struct to_body*)m->from->parsed)->tag_value;
	if (m->callid) id->call_id = m->callid->body;
	/* it doesn't depend on order local x remote tag - it is searched
	 * according to both (both directions) */
	return 0;
}

static dlg_t *new_dlg(struct sip_msg *m)
{
	dlg_t *dlg;
	dlg = (dlg_t*)shm_malloc(sizeof(dlg_t));
	if (!dlg) {
		ERR("can't allocate memory\n");
		return dlg;
	}
	
	/* TODO: create dlg_t from given message */
	return NULL;
}

int handle_save_dialog(struct sip_msg* m)
{
	dialog_info_t *info;
	dlg_id_t id;

	if (get_dialog_id(m, &id) < 0) {
		return -1;
	}
	
	ERR("trying to store dialog with AVPs somewhere\n");
	lock_dialog_mod();
	info = find_dialog(&id);
	if (!info) {
		info = (dialog_info_t*)shm_malloc(sizeof(dialog_info_t));
		if (!info) {
			ERR("can't allocate memory\n");
			unlock_dialog_mod();
			return -1;
		}

		info->dialog = new_dlg(m);
		
		/* store dialog */
		add_dialog(info);
	}
	
	/*  TODO: store status (AVPs, ...) from found dialog */
	unlock_dialog_mod();
	return 1;
}

int handle_load_dialog(struct sip_msg* m)
{ 
	dialog_info_t *info;
	dlg_id_t id;
	int res = 1;

	if (get_dialog_id(m, &id) < 0) {
		return -1;
	}
	
	ERR("trying to load dialog with AVPs\n");
	lock_dialog_mod();
	info = find_dialog(&id);
	if (info) {
		/*  TODO: restore status (AVPs, ...) from found dialog */
		res = 1;
	}
	else res = -1;
	unlock_dialog_mod();
	
	return res;
}

int handle_remove_dialog(struct sip_msg* _m)
{
	ERR("trying to remove dialog\n");
	lock_dialog_mod();
	/* try to find dialog
	 *  - if found destroy the structure and remove it from storage */
	unlock_dialog_mod();
	return 1;
}
