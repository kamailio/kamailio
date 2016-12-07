#ifndef __QSA_PARAMS_H
#define __QSA_PARAMS_H

#include <cds/sstr.h>
#include <cds/ptr_vector.h>
#include <cds/msg_queue.h>
#include <cds/ref_cntr.h>

#ifdef __cplusplus
extern "C" {
#endif
	
typedef struct _qsa_subscription_params_t {
	str_t name;
	str_t value; /* whatever */
	char buf[1];
} qsa_subscription_params_t;

#ifdef __cplusplus
}
#endif
	
#endif
