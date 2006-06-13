#ifdef SER
#ifdef DO_PROFILE

#include <cds/ser_profile.h>
#include "dprint.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

DEFINE_PROF_POINT(pa_handle_subscription)
DEFINE_PROF_POINT(pa_timer_presentity)
DEFINE_PROF_POINT(pa_timer_pdomain)
DEFINE_PROF_POINT(pa_response_generation)
	
DEFINE_PROF_POINT(rls_handle_subscription)
DEFINE_PROF_POINT(rls_timer_cb)
DEFINE_PROF_POINT(rls_is_simple_rls_target)
DEFINE_PROF_POINT(rls_query_rls_sevices)
DEFINE_PROF_POINT(rls_query_resource_list)
DEFINE_PROF_POINT(rls_have_flat_list)
	
DEFINE_PROF_POINT(tem_timer_cb)
DEFINE_PROF_POINT(tem_add_event)
DEFINE_PROF_POINT(tem_remove_event)
DEFINE_PROF_POINT(tem_do_step)

DEFINE_PROF_POINT(b2b_handle_notify)
	
void prof_trace(FILE *f, int pid, const char *s, profile_data_t a)	
{
	fprintf(f, "%d\t%30s\t%d\t%u\n", 
			pid, s, a.count, a.spent_time);
	if (a.start_count != a.stop_count) 
		fprintf(f, "%s, %d start_count != stop_count (%d != %d)\n", 
				s, pid, a.start_count, a.stop_count);
}

void prof_trace_nested(FILE *f, int pid, const char *s, profile_data_t a)	
{
	fprintf(f, "%d\t%29s*\t%d\t%u\n", 
			pid, s, a.count, a.spent_time);
	if (a.start_count != a.stop_count) 
		fprintf(f, "%s, %d start_count != stop_count (%d != %d)\n", 
				s, pid, a.start_count, a.stop_count);
}

#define trace(f, p, name)	prof_trace(f, p, #name , prof_point(name))
#define trace_nested(f, p, name)	prof_trace_nested(f, p, #name , prof_point(name))

void trace_func()
{
	pid_t p = getpid();
	FILE *f;

	f = fopen("/tmp/ser.profile", "at");
	if (!f) ERR("can't write into profile file\n");
	else {
		
		trace(f, p, pa_handle_subscription);
		trace(f, p, pa_timer_pdomain);
		trace_nested(f, p, pa_timer_presentity);
		trace_nested(f, p, pa_response_generation);
		
		trace(f, p, rls_handle_subscription);
		trace(f, p, rls_timer_cb);
		trace(f, p, rls_is_simple_rls_target);
		trace(f, p, rls_query_rls_sevices);
		trace(f, p, rls_query_resource_list);
		trace(f, p, rls_have_flat_list);
		
	/* 	trace_nested(f, p, tem_timer_cb); */
		trace(f, p, tem_add_event);
		trace(f, p, tem_remove_event);
		trace(f, p, tem_do_step);
		
		trace(f, p, b2b_handle_notify);
		
		fprintf(f, "%d\t%30s\t1\t%u\n", p, "all", get_prof_time());
		
		fclose(f);
	}
}
	
void ser_profile_init()
{
	WARN("initializing profiler\n");
	start_profile(trace_func);
}

#endif /* DO_PROFILE */

#endif /* SER */
