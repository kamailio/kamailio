/*
 * kz_fixup.h
 *
 *  Created on: Aug 2, 2014
 *      Author: root
 */

#ifndef KZ_FIXUP_H_
#define KZ_FIXUP_H_

int fixup_kz_amqp(void** param, int param_no);
int fixup_kz_amqp_free(void** param, int param_no);

int fixup_kz_amqp4(void** param, int param_no);
int fixup_kz_amqp4_free(void** param, int param_no);

int fixup_kz_json(void** param, int param_no);
int fixup_kz_json_free(void** param, int param_no);

int fixup_kz_amqp_encode(void** param, int param_no);
int fixup_kz_amqp_encode_free(void** param, int param_no);



#endif /* KZ_FIXUP_H_ */
