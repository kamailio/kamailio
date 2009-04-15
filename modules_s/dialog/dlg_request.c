#include "dlg_request.h"
#include "dlg_mod_internal.h"

/*
 * Send an initial request that will start a dialog with given route header
 * the dialog must be created before!!!
 */
int request_outside(str* method, str* headers, str* body, dlg_t* dialog, transaction_cb cb, void* cbp)
{
	uac_req_t	uac_r;

	/* check parameters */
	if ((!dialog) || (!method)) goto err;
	if ((method->len < 0) || (!method->s)) goto err;
	if (dialog->state != DLG_NEW) {
		LOG(L_ERR, "req_within: Dialog is not in DLG_NEW state\n");
		goto err;
	}

	if (!dialog->hooks.next_hop) {
		/* FIXME: this is only experimental - hooks are calculated only when
		 * next hop is not known */
		if (tmb.calculate_hooks(dialog) < 0) {
			LOG(L_ERR, "Error while calculating hooks\n");
			return -2;
		}
	}

	set_uac_req(&uac_r,
			method,
			headers,
			body,
			dialog,
			TMCB_LOCAL_COMPLETED,
			cb,
			cbp);

	return tmb.t_uac(&uac_r);

 err:
/*	if (cbp) shm_free(cbp);*/	/* !!! never do this automaticaly??? !!! */

	/* call the callback? Probably not because we can be in locked section
	 * and the callback can try to lock it too. */
	return -1;
}

/*
 * Send a message within a dialog
 */
int request_inside(str* method, str* headers, str* body, dlg_t* dialog, transaction_cb completion_cb, void* cbp)
{
	uac_req_t	uac_r;

	if (!method || !dialog) {
		LOG(L_ERR, "req_within: Invalid parameter value\n");
		goto err;
	}

	if (dialog->state != DLG_CONFIRMED) {
		LOG(L_ERR, "req_within: Dialog is not confirmed yet\n");
		goto err;
	}

	if ((method->len == 3) && (!memcmp("ACK", method->s, 3))) goto send;
	if ((method->len == 6) && (!memcmp("CANCEL", method->s, 6))) goto send;
	dialog->loc_seq.value++; /* Increment CSeq */
 send:
	set_uac_req(&uac_r,
			method,
			headers,
			body,
			dialog,
			TMCB_LOCAL_COMPLETED,
			completion_cb,
			cbp);

	return tmb.t_uac(&uac_r);

 err:
/*	if (cbp) shm_free(cbp); */ /* !!! never !!! */
	return -1;
}
