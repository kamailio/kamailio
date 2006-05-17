#include "time_event_manager.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

#include <stdio.h>
#include <string.h>
#include "trace.h"

typedef struct {
	time_event_manager_t *first;
	time_event_manager_t *last;
	gen_lock_t structure_mutex;
} tem_info_t;

static tem_info_t *tem_info = NULL;

static void tem_do_step(time_event_manager_t *tem);

static void tem_timer_cb(unsigned int ticks, void *param)
{
	time_event_manager_t *e, *n;

	PROF_START(tem_timer_cb)
	if (!tem_info) return;

	e = tem_info->first;
	while (e) {
		n = e->next;
		if (--e->process_timer_counter == 0) {
			tem_do_step(e);
			e->process_timer_counter = e->atomic_time;
		}
		e = n;
	}
	PROF_STOP(tem_timer_cb)
}

int time_event_management_init()
{
	if (tem_info) return 0; /* already initialized */
	
	tem_info = (tem_info_t *)mem_alloc(sizeof(tem_info_t));
	if (!tem_info) {
		LOG(L_ERR, "time_event_management_init(): can't allocate shared memory\n");
		return -1;
	}
	tem_info->first = NULL;
	tem_info->last = NULL;
	lock_init(&tem_info->structure_mutex);
	
	/* register a SER timer */
	if (register_timer(tem_timer_cb, NULL, 1) < 0) {
		LOG(L_ERR, "time_event_management_init(): can't register timer\n");
		return -1;
	}
	return 0;
}

void time_event_management_destroy()
{
	time_event_manager_t *e, *n;
	tem_info_t *ti = tem_info;

	tem_info = NULL;
	/* F I X M E: unregister SER timer ? */
	
	if (!ti) return;

	e = ti->first;
	while (e) {
		n = e->next;
		tem_destroy(n);
		e = n;
	}

	mem_free(ti);
}

int tem_init(time_event_manager_t *tm, unsigned int atomic_time, 
		unsigned int slot_cnt, int enable_delay, gen_lock_t *mutex)
{
	if (!tm) return -1;

	tm->tick_counter = 0;
	tm->atomic_time = atomic_time;
	tm->slot_cnt = slot_cnt;
	tm->enable_delay = enable_delay;
	tm->mutex = mutex;
	tm->time_slots = (time_event_slot_t *)mem_alloc(slot_cnt * sizeof(time_event_slot_t));
	if (!tm->time_slots) {
		LOG(L_ERR, "can't initialize time event manager slots\n");
		return -1;
	}
	memset(tm->time_slots, 0, slot_cnt * sizeof(time_event_slot_t));

	tm->next = NULL;
	tm->process_timer_counter = atomic_time;
	
	lock_get(&tem_info->structure_mutex);

	tm->prev = tem_info->last;
	if (tem_info->last) tem_info->last->next = tm;
	else tem_info->first = tm;
	tem_info->last = tm;
	
	lock_release(&tem_info->structure_mutex);

	return 0;
}

time_event_manager_t *tem_create(unsigned int atomic_time, unsigned int slot_cnt, int enable_delay, gen_lock_t *mutex)
{
	time_event_manager_t *tm;
	
	tm = (time_event_manager_t*)mem_alloc(sizeof(time_event_manager_t));
	if (!tm) {
		LOG(L_ERR, "can't allocate time event manager\n");
		return tm;
	}

	if (tem_init(tm, atomic_time, slot_cnt, enable_delay, mutex) != 0) {
		mem_free(tm);
		return NULL;
	}

	return tm;
}

void tem_destroy(time_event_manager_t *tem)
{
	if (tem) {
		lock_get(&tem_info->structure_mutex);
		
		if (tem->prev) tem->prev->next = tem->next;
		else tem_info->first = tem->next;
		if (tem->next) tem->next->prev = tem->prev;
		else tem_info->last = tem->prev;
		
		lock_release(&tem_info->structure_mutex);
		
		if (tem->time_slots) mem_free(tem->time_slots);
		mem_free(tem);
	}
}

void tem_add_event(time_event_manager_t *tem, unsigned int action_time, time_event_data_t *te)
{
	if (tem->mutex) lock_get(tem->mutex);
	tem_add_event_nolock(tem, action_time, te);
	if (tem->mutex) lock_release(tem->mutex);
}

void tem_remove_event(time_event_manager_t *tem, time_event_data_t *te)
{
	if (tem->mutex) lock_get(tem->mutex);
	tem_remove_event_nolock(tem, te);
	if (tem->mutex) lock_release(tem->mutex);
}

void tem_add_event_nolock(time_event_manager_t *tem, unsigned int action_time, time_event_data_t *te)
{
	unsigned int tick, s;
	
	PROF_START(tem_add_event)
	if (!te) return;
	tick = action_time / tem->atomic_time;
	if ((tem->enable_delay) && (action_time % tem->atomic_time > 0)) {
		/* rather call the action later than before */
		tick++;
	}
	if (tick <= 0) tick = 1; /* never add to current slot (? only if not processing ?)*/
	tick += tem->tick_counter;
	s = tick % tem->slot_cnt;

	te->next = NULL;
	te->prev = tem->time_slots[s].last;
	if (tem->time_slots[s].last) 
		tem->time_slots[s].last->next = te;
	else 
		tem->time_slots[s].first = te;
	tem->time_slots[s].last = te;

	te->tick_time = tick;
	PROF_STOP(tem_add_event)
}

void tem_remove_event_nolock(time_event_manager_t *tem, time_event_data_t *te)
{
	time_event_slot_t *slot;

	PROF_START(tem_remove_event)
	if (!te) return;
	
	slot = &tem->time_slots[te->tick_time % tem->slot_cnt];
	
	if (te->prev) te->prev->next = te->next;
	else slot->first = te->next;
	if (te->next) te->next->prev = te->prev;
	else slot->last = te->prev;
	te->next = NULL;
	te->prev = NULL;
	PROF_STOP(tem_remove_event)
}

static void tem_do_step(time_event_manager_t *tem) 
{
	time_event_data_t *e, *n, *unprocessed_first, *unprocessed_last;
	time_event_slot_t *slot;

	PROF_START(tem_do_step)
	if (tem->mutex) lock_get(tem->mutex);
	
	unprocessed_first = NULL;
	unprocessed_last = NULL;
	slot = &tem->time_slots[tem->tick_counter % tem->slot_cnt];
	e = slot->first;
	while (e) {
		n = e->next;
		if (e->tick_time == tem->tick_counter) {
			if (e->cb) e->cb(e);
			/* the pointer to this element is forgotten - it MUST be
			 * freed in the callback function */
		} 
		else {
			/* it is not the right time => give it into unprocessed events */
			e->prev = unprocessed_last;
			e->next = NULL;
			if (unprocessed_last) unprocessed_last->next = e;
			else unprocessed_first = e;
			unprocessed_last = e;
		}
		e = n;
	}

	slot->first = unprocessed_first;
	slot->last = unprocessed_last;
	tem->tick_counter++;
	
	if (tem->mutex) lock_release(tem->mutex);
	PROF_STOP(tem_do_step)
}

