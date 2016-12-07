#ifndef __TIME_EVENT_MANAGER_H
#define __TIME_EVENT_MANAGER_H

#include "../../lock_ops.h"
#include "../../timer.h"

struct _time_event_data_t;

typedef void(*time_event_func)(struct _time_event_data_t *s);

typedef struct _time_event_data_t {
	unsigned int tick_time;
	
	/** callback function */
	time_event_func cb;
	/** callback function argument */
	void *cb_param;
	/** callback function argument */
	void *cb_param1;
	
	/** next element in time slot */
	struct _time_event_data_t *next;
	/** previous element in time slot */
	struct _time_event_data_t *prev;
} time_event_data_t;

typedef struct _time_event_slot_t {
	time_event_data_t *first, *last;
} time_event_slot_t;

typedef struct _time_event_manager_t {
	time_event_slot_t *time_slots;
	unsigned int slot_cnt;
	/** atomic time in seconds */
	unsigned int atomic_time;
	/** allow the event to be "called" after its time */
	int enable_delay;
	/** counts ticks - this is an absolute value "timer" */
	unsigned int tick_counter;
	/** mutex is taken from parent (locking must be common - deadlock prevention) */
	gen_lock_t *mutex;
	/** count of seconds after which should be called this timer's step */

	unsigned int process_timer_counter;
	struct _time_event_manager_t *next;
	struct _time_event_manager_t *prev;
} time_event_manager_t;

time_event_manager_t *tem_create(unsigned int atomic_time, unsigned int slot_cnt, int enable_delay, gen_lock_t *mutex);
int tem_init(time_event_manager_t *tm, unsigned int atomic_time, unsigned int slot_cnt, int enable_delay, gen_lock_t *mutex);
void tem_destroy(time_event_manager_t *tem);
void tem_add_event(time_event_manager_t *tem, unsigned int action_time, time_event_data_t *te);
void tem_remove_event(time_event_manager_t *tem, time_event_data_t *te);
void tem_add_event_nolock(time_event_manager_t *tem, unsigned int action_time, time_event_data_t *te);
void tem_remove_event_nolock(time_event_manager_t *tem, time_event_data_t *te);

int time_event_management_init();
void time_event_management_destroy();

#endif
