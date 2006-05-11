#ifndef __SER_PROFILE
#define __SER_PROFILE

#ifdef SER

/* declarations of watched profile points */

#include <cds/simple_profile.h>

DECLARE_PROF_POINT(pa_handle_subscription)
DECLARE_PROF_POINT(pa_timer_presentity)
DECLARE_PROF_POINT(pa_timer_pdomain)
DECLARE_PROF_POINT(pa_response_generation)
	
DECLARE_PROF_POINT(rls_handle_subscription)
DECLARE_PROF_POINT(rls_timer_cb)
DECLARE_PROF_POINT(rls_is_simple_rls_target)
DECLARE_PROF_POINT(rls_query_rls_sevices)
DECLARE_PROF_POINT(rls_query_resource_list)
DECLARE_PROF_POINT(rls_have_flat_list)
DECLARE_PROF_POINT(tem_timer_cb)
DECLARE_PROF_POINT(tem_add_event)
DECLARE_PROF_POINT(tem_remove_event)
DECLARE_PROF_POINT(tem_do_step)

DECLARE_PROF_POINT(b2b_handle_notify)

/* do NOT use directly this */
void ser_profile_init();

#define SER_PROFILE_INIT	ser_profile_init();

#endif

#endif
