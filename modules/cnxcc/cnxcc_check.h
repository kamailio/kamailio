/*
 * cnxcc_check.h
 *
 *  Created on: Dec 10, 2012
 *      Author: carlos
 */

#ifndef CNXCC_CHECK_H_
#define CNXCC_CHECK_H_

#define SAFE_ITERATION_THRESHOLD 5000

void check_calls_by_time(unsigned int ticks, void *param);
void check_calls_by_money(unsigned int ticks, void *param);

#endif /* CNXCC_CHECK_H_ */
