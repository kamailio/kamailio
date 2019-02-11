/*
 * common.h
 *
 *  Created on: 10 Apr 2013
 *      Author: jaybeepee
 */

#ifndef COMMON_H_
#define COMMON_H_

#include "diameter.h"
#include "diameter_api.h"


int get_result_code(AAAMessage* msg);
int get_accounting_record_type(AAAMessage* msg);


#endif /* COMMON_H_ */
