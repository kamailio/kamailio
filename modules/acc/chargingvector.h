/* 
 * Support for rfc3455 P-Charging-Vector
 * - parse charging vector from SIP message
 * - generate new unique charging vector
 * - can remove charging vector
 *
 * pseudo variables are exported and enable R ondly access to charging vector fields
 * $pcv = whole field
 * $pcv.value = icid-value field (see RFC3455 section 5.6)
 * $pcv.genaddr = icid-generated-at field (see RFC3455 section 5.6)
 *
 * to be supported
 * $pcv.orig
 * $pcv.term
 */
int acc_handle_pcv(struct sip_msg *msg, char *flags, char *str2);

int pv_get_charging_vector(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);