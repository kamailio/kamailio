#ifndef __SIMPLE_PROFILE_H
#define __SIMPLE_PROFILE_H

#ifdef __cplusplus
extern "C" {
#endif
	
typedef void(*trace_f)();

int start_profile(trace_f tf);
int stop_profile();

/* do NOT use this directly ! */
extern unsigned int tick_counter;

#define get_prof_time()	tick_counter

typedef struct {
	int count;
	int start_count, stop_count;
	unsigned int spent_time;
} profile_data_t;

#define DEFINE_PROF_POINT(name) profile_data_t prof_##name = { 0, 0, 0 };
#define DECLARE_PROF_POINT(name) extern profile_data_t prof_##name;

#define prof_point(name)	prof_##name
#define PROF_START_DECL(name)	int _prof_act_##name;
#define PROF_START_BODY(name)	prof_point(name).count++; \
							prof_point(name).start_count++; \
							_prof_act_##name = get_prof_time();

#define PROF_START(name)	int _prof_act_##name; \
							prof_point(name).count++; \
							prof_point(name).start_count++; \
							_prof_act_##name = get_prof_time();

#define PROF_STOP(name)	prof_point(name).stop_count++; \
						prof_point(name).spent_time += get_prof_time() - _prof_act_##name;

#define prof_return(a, val)	prof_stop(a) return val;

#ifdef __cplusplus
}
#endif
 
#endif
