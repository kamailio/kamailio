#ifndef CLIENT_RF_IMS_RO_H
#define CLIENT_RF_IMS_RO_H

#include "../../mod_fix.h"
#include "../cdp/diameter_api.h"
#include "ro_session_hash.h"

struct interim_ccr {
	struct ro_session* ro_session;
	int new_credit;
	int credit_valid_for;
	unsigned int is_final_allocation;
};

void credit_control_session_callback(int event, void* session);
void remove_aaa_session(str *session_id);
int Ro_Send_CCR(struct sip_msg *msg, struct dlg_cell *dlg, int dir, int reservation_units, 
	    str *incoming_trunk_id, str *outgoing_trunk_id, str *enb_cell_id, cfg_action_t* action, unsigned int tindex, unsigned int tlabel);
void send_ccr_interim(struct ro_session* ro_session, unsigned int used, unsigned int reserve);
void send_ccr_stop(struct ro_session *ro_session);
int get_direction_as_int(str* direction);

#endif /* CLIENT_RF_IMS_RO_H */
