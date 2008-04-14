#include <stdio.h>
#include <cds/simple_profile.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>

/* FIXME: only for testing */
/*#include "dprint.h"*/

#define trace_signal SIGTRAP

typedef void (*_sig_t) (int);

unsigned int tick_counter = 0;
static struct sigaction old_sigprof_action;
static _sig_t old_sigx_action;
static int initialized = 0;
static trace_f trace_function = NULL;

int reset_timer()
{
	struct itimerval tv;
	int res;
	
	tv.it_interval.tv_sec = 0;
	tv.it_interval.tv_usec = 1000; 
	/*tv.it_interval.tv_usec = 10 * 1000; */
	tv.it_value.tv_sec = tv.it_interval.tv_sec;
	tv.it_value.tv_usec = tv.it_interval.tv_usec;

	res = setitimer(ITIMER_PROF, &tv, NULL);

	return res;
}

void prof_handler(int a, siginfo_t *info, void *b)
{
	tick_counter++;
/*	LOG(L_ERR, "PROFILE HANDLER called\n"); */
	/* reset_timer(); */
}

void trace_handler(int a)
{
	if (trace_function) trace_function();
}

int start_profile(trace_f tf)
{
	struct sigaction action;
	int res;

	if (initialized) return 1;
	initialized = 1;
	trace_function = tf;

	memset(&action, 0, sizeof(action));
	action.sa_sigaction = prof_handler;
    sigemptyset(&action.sa_mask);
	action.sa_flags = SA_SIGINFO;
	res = sigaction(SIGPROF, &action, &old_sigprof_action);
	if (res != 0) {
		/* ERROR_LOG("can't set signal handle (%d)\n", res);*/
		return -1;
	}
	
	old_sigx_action = signal(trace_signal, &trace_handler);

	res = reset_timer();
	if (res != 0) {
		/* ERROR_LOG("can't set itimer (%d)\n", res);*/
		signal(trace_signal, old_sigx_action);
		sigaction(SIGPROF, &old_sigprof_action, NULL);
		return -1;
	}

	return 0;
}

int stop_profile()
{
	if (initialized) {
		initialized = 0;
		signal(trace_signal, old_sigx_action);
		return sigaction(SIGPROF, &old_sigprof_action, NULL);
	}
	else return -1;
}

