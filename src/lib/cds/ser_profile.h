#ifndef __SER_PROFILE
#define __SER_PROFILE

#ifdef SER

#ifdef DO_PROFILING

#include <cds/simple_profile.h>

/* declarations of watched profile points */

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

#else /* don't profile */

#define SER_PROFILE_INIT
#define PROF_START(name)
#define PROF_START_BODY(name)
#define PROF_START_DECL(name)
#define PROF_STOP(name)

#endif /* DO_PROFILING */

#endif /* SER */


#endif
