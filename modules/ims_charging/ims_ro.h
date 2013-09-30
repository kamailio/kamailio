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
int Ro_Send_CCR(struct sip_msg *msg, str* direction, str* charge_type, str* unit_type, int reservation_units, cfg_action_t* action, unsigned int tindex, unsigned int tlabel);
//void send_ccr_interim(struct ro_session *ro_session, str* from_uri, str *to_uri, int *new_credit, int *credit_valid_for, unsigned int used, unsigned int reserve, unsigned int *is_final_allocation);
void send_ccr_interim(struct ro_session* ro_session, unsigned int used, unsigned int reserve);
void send_ccr_stop(struct ro_session *ro_session);


#endif /* CLIENT_RF_IMS_RO_H */
