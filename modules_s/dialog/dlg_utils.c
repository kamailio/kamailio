#include "../../parser/parse_rr.h"
#include "dlg_mod_internal.h"
#include "dlg_utils.h"
#include "dlg_request.h"
#include "serialize_dlg.h"
#include <cds/hash_table.h>
/* #include <cds/hash_functions.h> */

int preset_dialog_route(dlg_t* dialog, str *route)
{
	rr_t *old_r, *r = NULL;
	int res;

	/* check parameters */
	if ((!dialog) || (is_str_empty(route))) {
		ERR("bad parameters\n");
		return -1;
	}
	if (dialog->state != DLG_NEW) {
		ERR("Dialog is not in DLG_NEW state\n");
		return -1;
	}
	
	if (parse_rr_body(route->s, route->len, &r) < 0) {
		ERR("can't parse given route\n");
		return -1;
	}

	if (!r) {
		ERR("empty route\n");
		return -1;
	}

	old_r = dialog->route_set;
	dialog->route_set = NULL;
	res = shm_duplicate_rr(&dialog->route_set, r);
	if (r) free_rr(&r);
	if (res < 0) {
		/* return old routeset to its place */
		dialog->route_set = old_r;
		ERR("can't duplicate route\n");
		return -1;
	}
	
	/* free old route */
	if (old_r) shm_free_rr(&old_r);

	res = tmb.calculate_hooks(dialog);
	if (res < 0) {
		ERR("Error while calculating hooks\n");
		return -2;
	}

	return 0;
}

int bind_dlg_mod(dlg_func_t *dst)
{
	if (!dst) return -1;
/*	dst->db_store = db_store_dlg;
	dst->db_load = db_load_dlg;*/

	memset(dst, 0, sizeof(*dst));
	
	dst->serialize = serialize_dlg;
	dst->dlg2str = dlg2str;
	dst->str2dlg = str2dlg;
	dst->preset_dialog_route = preset_dialog_route;
	dst->request_outside = request_outside;
	dst->request_inside = request_inside;
	dst->hash_dlg_id = hash_dlg_id;
	dst->cmp_dlg_ids = cmp_dlg_ids;
	
	return 0;
}

int cmp_dlg_ids(dlg_id_t *a, dlg_id_t *b)
{
	if (!a) {
		if (!b) return -1;
		else return 0;
	}
	if (!b) return 1;

	if (str_case_equals(&a->call_id, &b->call_id) != 0) return 1;
	if (str_case_equals(&a->rem_tag, &b->rem_tag) != 0) return 1; /* case sensitive ? */
	if (str_case_equals(&a->loc_tag, &b->loc_tag) != 0) return 1; /* case sensitive ? */
	return 0;
}

unsigned int hash_dlg_id(dlg_id_t *id)
{
	char tmp[512];
	int len;
	
	if (!id) return 0;
	
	len = snprintf(tmp, sizeof(tmp), "%.*s%.*s%.*s",
			FMT_STR(id->call_id),
			FMT_STR(id->rem_tag),
			FMT_STR(id->loc_tag));

	return rshash(tmp, len);
}

