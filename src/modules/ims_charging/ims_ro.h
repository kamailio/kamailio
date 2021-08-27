#ifndef CLIENT_RF_IMS_RO_H
#define CLIENT_RF_IMS_RO_H

#include "../../core/mod_fix.h"
#include "../cdp/diameter_api.h"
#include "../ims_dialog/dlg_hash.h"
#include "ro_session_hash.h"

typedef enum {
    VS_TERMCODE = 3,
    VS_TERMREASON = 2
} vs_term_avp;

struct interim_ccr {
	struct ro_session* ro_session;
	int new_credit;
	int credit_valid_for;
	unsigned int is_final_allocation;
};

void credit_control_session_callback(int event, void* session);
void remove_aaa_session(str *session_id);
int Ro_Send_CCR(struct sip_msg *msg, struct dlg_cell *dlg, int dir, int reservation_units, 
	    str *incoming_trunk_id, str *outgoing_trunk_id, str *enb_cell_id, void* action, unsigned int tindex, unsigned int tlabel);
long get_current_time_micro();
void send_ccr_interim(struct ro_session* ro_session, unsigned int used, unsigned int reserve);
void send_ccr_stop_with_param(struct ro_session *ro_session, unsigned int code, str* reason);
int get_direction_as_int(str* direction);

void init_custom_user(pv_spec_t *custom_user_avp);
void init_app_provided_party(pv_spec_t *app_provided_party_avp_p);

#endif /* CLIENT_RF_IMS_RO_H */
