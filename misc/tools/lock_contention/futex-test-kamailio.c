/*
* Copyright (C) 2025 GILAWA Ltd
*
* This file is part of Kamailio, a free SIP server.
*
* SPDX-License-Identifier: GPL-2.0-or-later
*
* Kamailio is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version
*
* Kamailio is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdatomic.h>

/*  Include headers for the futex syscall */
#ifdef SYS_FUTEX
#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/time.h>
#endif

/*  Fast lock */
#ifdef FAST_LOCK
#include "fastlock.h"
#endif

/*  Kamailio futex implementation */
#ifdef KAMAILIO_FUTEX
#include "futexlock.h"
#endif

#define HTABLE_SIZE 256		// Number of buckets in the hash table
#define PAYLOAD_SIZE 1024	// 1KB payload for each record
#define HOT_BUCKET_INDEX 42 // All threads will target this single bucket

// --- Futex Lock Implementation ---
#ifdef SYS_FUTEX
/**
 * @brief The core futex lock structure.
 */
typedef struct
{
	volatile int lock_status;
} futex_lock_t;

/**
 * @brief Performs the futex syscall.
 */
static long futex(int *uaddr, int futex_op, int val,
		const struct timespec *timeout, int *uaddr2, int val3)
{
	return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

/**
 * @brief Initializes a futex lock to an unlocked state.
 */
static void futex_lock_init(futex_lock_t *lock)
{
	lock->lock_status = 0;
}

/**
 * @brief Acquires a futex lock, tracking contention statistics.
 */
static void futex_lock(futex_lock_t *lock, unsigned long long *waits_counter,
		unsigned long long *contentions_counter)
{
	int expected_unlocked = 0;

	// Fast path: attempt to acquire the lock atomically.
	if(__atomic_compare_exchange_n(&lock->lock_status, &expected_unlocked, 1, 0,
			   __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
		return; // Lock acquired
	}
	(*contentions_counter)++;

	// Contention path: lock is held, so we must wait.
	while(1) {

		// Wait on the futex. The kernel will sleep the thread.
		// Using FUTEX_WAIT for inter-process compatibility, as Kamailio would.
		futex((int *)&lock->lock_status, FUTEX_WAIT, 1, NULL, NULL, 0);
		(*waits_counter)++;

		// After waking up, try to acquire the lock again.
		expected_unlocked = 0;
		if(__atomic_compare_exchange_n(&lock->lock_status, &expected_unlocked,
				   1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
			return; // Lock acquired after waiting
		}
	}
}

/**
 * @brief Releases a futex lock.
 */
static void futex_unlock(futex_lock_t *lock)
{
	// Atomically release the lock.
	__atomic_store_n(&lock->lock_status, 0, __ATOMIC_SEQ_CST);

	// Wake up one waiting thread.
	// Using FUTEX_WAKE for inter-process compatibility.
	futex((int *)&lock->lock_status, FUTEX_WAKE, 1, NULL, NULL, 0);
}
#endif

/* --- Fast Lock Implementation --- */
/* Wrapper for fast_lock with contention/wait tracking */
#ifdef FAST_LOCK
static inline void fast_lock_tracked(fl_lock_t *lock,
		unsigned long long *waits_counter,
		unsigned long long *contentions_counter)
{
	// // Try fast path first
	if(try_lock(lock) == 0) {
		// Lock acquired immediately
		// putchar('/'); // Optional: indicate lock acquired
		return;
	}
	// membar_getlock();

	// Contention detected
	// if(contentions_counter)

	// Wait for the lock
	// This will spin until the lock is acquired
	// get_lock(lock);
	(*contentions_counter)++;

	while(tsl(lock)) {
		sched_yield();
		usleep(10);
		(*waits_counter)++;
		// We assume that if we didn't get the lock immediately, we had to wait
		// if(waits_counter)
		// 	(*waits_counter)++;
		// // putchar('.'); // Optional: indicate waiting
	}
	membar_getlock();
	return;
}
#endif

/* Kamailio Futex Implementation */
/* Wrapper for lock and track */
#ifdef KAMAILIO_FUTEX
static inline void kamailio_futex_tracked(futex_lock_t *lock,
		unsigned long long *waits_counter,
		unsigned long long *contentions_counter)
{
	int v;
#ifdef ADAPTIVE_WAIT
	register int i = ADAPTIVE_WAIT_LOOPS;

retry:
#endif

	v = atomic_cmpxchg(lock, 0, 1); /* lock if 0 */
	if(likely(v == 0)) {			/* optimize for the uncontended case */
		/* success */
		membar_enter_lock();
		return;
	} else if(unlikely(v == 2)) { /* if contended, optimize for the one waiter
								case */
		(*contentions_counter)++;
		/* waiting processes/threads => add ourselves to the queue */
		do {
			(*waits_counter)++;
			sys_futex(&(lock)->val, FUTEX_WAIT, 2, 0, 0, 0);
			v = atomic_get_and_set(lock, 2);
		} while(v);
	} else {
		(*contentions_counter)++;
		/* v==1 */
#ifdef ADAPTIVE_WAIT
		if(i > 0) {
			i--;
			goto retry;
		}
#endif
		v = atomic_get_and_set(lock, 2);
		while(v) {
			(*waits_counter)++;
			sys_futex(&(lock)->val, FUTEX_WAIT, 2, 0, 0, 0);
			v = atomic_get_and_set(lock, 2);
		}
	}
	membar_enter_lock();
}
#endif

// --- Hash Table Implementation (htable) ---

/**
 * @brief Represents a single slot/bucket in the hash table.
 * It contains a lock and a data payload.
 */
typedef struct
{
#if defined(FAST_LOCK)
	fl_lock_t lock;
#elif defined(SYS_FUTEX)
	futex_lock_t lock;
#elif defined(KAMAILIO_FUTEX)
	futex_lock_t lock;
#endif
	char payload[PAYLOAD_SIZE];
} htable_slot_t;

/**
 * @brief Represents the entire hash table structure.
 */
typedef struct
{
	int size;
	htable_slot_t *slots;
} htable_t;

#ifdef SYS_FUTEX
#define LOCK(lock, waits_counter, contentions_counter) \
	futex_lock(lock, waits_counter, contentions_counter)
#define UNLOCK(lock) futex_unlock(lock)
#elif defined FAST_LOCK
#define LOCK(lock, waits_counter, contentions_counter) \
	fast_lock_tracked(lock, waits_counter, contentions_counter)
#define UNLOCK(lock) release_lock(lock)
#elif defined KAMAILIO_FUTEX
#define LOCK(lock, waits_counter, contentions_counter) \
	kamailio_futex_tracked(lock, waits_counter, contentions_counter)
#define UNLOCK(lock) futex_release(lock)
#endif

/**
 * @brief Initializes the hash table, including all per-bucket locks and payloads.
 * @param table The hash table to initialize.
 * @param size The number of buckets for the table.
 * @return 0 on success, -1 on failure.
 */
int htable_init(htable_t *table, int size)
{
	int i;
	table->size = size;
	table->slots = malloc(size * sizeof(htable_slot_t));
	if(table->slots == NULL) {
		perror("Failed to allocate memory for htable slots");
		return -1;
	}

	for(i = 0; i < size; i++) {
#if defined(FAST_LOCK)
		init_lock(table->slots[i].lock);
#elif defined(SYS_FUTEX)
		futex_lock_init(&table->slots[i].lock);
#elif defined(KAMAILIO_FUTEX)
		futex_init(&table->slots[i].lock);
#endif
		// Initialize payload with some data
		memset(table->slots[i].payload, 0, PAYLOAD_SIZE);
	}
	return 0;
}

/**
 * @brief Frees the memory used by the hash table.
 */
void htable_destroy(htable_t *table)
{
	if(table && table->slots) {
		free(table->slots);
		table->slots = NULL;
	}
}


// --- Threading and Statistics ---

static atomic_int keep_running = 1;

/**
 * @brief Holds the statistics for a single worker thread.
 */
typedef struct
{
	int thread_id;
	unsigned long long lock_count;
	unsigned long long wait_count;
	unsigned long long contention_count;
	htable_t *shared_htable;
} thread_data_t;

/**
 * @brief The main function for each worker thread.
 *
 * Each thread continuously locks a single "hot" bucket, writes a 1KB
 * payload to simulate work, and then unlocks it.
 *
 * @param arg A pointer to the thread_data_t for this thread.
 * @return NULL.
 */
void *worker_thread_func(void *arg)
{
	thread_data_t *data = (thread_data_t *)arg;
	htable_t *ht = data->shared_htable;
	// All threads target the same bucket to simulate high contention on one item.
	int bucket_index = HOT_BUCKET_INDEX;
	char local_payload[PAYLOAD_SIZE];

	data->lock_count = 0;
	data->wait_count = 0;
	data->contention_count = 0;

	// Prepare some dummy data to write.
	memset(local_payload, (char)data->thread_id, PAYLOAD_SIZE);

	while(atomic_load_explicit(&keep_running, memory_order_relaxed)) {
		// 1. Lock the single, specific "hot" bucket.
		// futex_lock(&ht->slots[bucket_index].lock, &data->wait_count,
		// 		&data->contention_count);
		LOCK(&ht->slots[bucket_index].lock, &data->wait_count,
				&data->contention_count);
		// 2. Critical section: Simulate writing a large record.
		// This makes the critical section longer, increasing the chance of contention.
		memcpy(ht->slots[bucket_index].payload, local_payload, PAYLOAD_SIZE);
		atomic_fetch_add_explicit(&data->lock_count, 1, memory_order_relaxed);

		// 3. Unlock the bucket.
		// futex_unlock(&ht->slots[bucket_index].lock);
		UNLOCK(&ht->slots[bucket_index].lock);
	}

	return NULL;
}

/**
 * @brief Prints the final aggregated report.
 */
void print_report(int num_threads, int duration, thread_data_t *all_thread_data)
{
	unsigned long long total_locks = 0;
	unsigned long long total_waits = 0;
	unsigned long long total_contentions = 0;
	int i;

	printf("\n--- Futex Contention Test Report (High Contention "
		   "Simulation) "
		   "---\n");
	printf("Duration: %d seconds\n", duration);
	printf("Threads:  %d\n", num_threads);
	printf("HTable Buckets: %d (All threads targeting bucket %d)\n",
			HTABLE_SIZE, HOT_BUCKET_INDEX);
	printf("---------------------------------------------------------------"
		   "---"
		   "\n");

	for(i = 0; i < num_threads; i++) {
		total_locks += all_thread_data[i].lock_count;
		total_waits += all_thread_data[i].wait_count;
		total_contentions += all_thread_data[i].contention_count;
	}

	printf("Total Locks Acquired: %llu\n", total_locks);
	printf("Total Futex Waits:    %llu (A thread went to sleep)\n",
			total_waits);
	printf("Total Contentions:    %llu (A thread failed to acquire lock)\n",
			total_contentions);
	printf("---------------------------------------------------------------"
		   "---"
		   "\n");

	if(duration > 0) {
		printf("Locks per second:     %.2f\n", (double)total_locks / duration);
		printf("Waits per second:     %.2f\n", (double)total_waits / duration);
		printf("Contentions per sec:  %.2f\n",
				(double)total_contentions / duration);
	}
	printf("---------------------------------------------------------------"
		   "---"
		   "\n\n");
}


// --- Main Application ---

int main(int argc, char *argv[])
{
	int duration;
	int num_threads;
	htable_t shared_htable;
	pthread_t *threads;
	thread_data_t *thread_data;
	int i;

	if(argc != 3) {
		fprintf(stderr, "Usage: %s <seconds> <threads>\n", argv[0]);
		return 1;
	}

	duration = atoi(argv[1]);
	num_threads = atoi(argv[2]);

	if(duration <= 0 || num_threads <= 0) {
		fprintf(stderr, "Error: Duration and thread count must be positive "
						"integers.\n");
		return 1;
	}

	threads = malloc(num_threads * sizeof(pthread_t));
	thread_data = malloc(num_threads * sizeof(thread_data_t));
	if(threads == NULL || thread_data == NULL) {
		perror("Failed to allocate memory for thread management");
		free(threads);
		free(thread_data);
		return 1;
	}

	// Initialize the shared hash table
	if(htable_init(&shared_htable, HTABLE_SIZE) != 0) {
		free(threads);
		free(thread_data);
		return 1;
	}

	printf("Starting htable high-contention test for %d seconds with %d "
		   "threads.\n",
			duration, num_threads);

	// Create and launch threads
	for(i = 0; i < num_threads; i++) {
		thread_data[i].thread_id = i;
		thread_data[i].shared_htable = &shared_htable;
		if(pthread_create(
				   &threads[i], NULL, worker_thread_func, &thread_data[i])
				!= 0) {
			perror("Failed to create thread");
			htable_destroy(&shared_htable);
			free(threads);
			free(thread_data);
			return 1;
		}
	}

	// Let the threads run for the specified duration
	sleep(duration);

	// Signal threads to stop
	atomic_store_explicit(&keep_running, 0, memory_order_relaxed);

	printf("Stopping threads and gathering results...\n");

	// Wait for all threads to complete
	for(i = 0; i < num_threads; i++) {
		pthread_join(threads[i], NULL);
	}

	// Print the final report
	print_report(num_threads, duration, thread_data);

	// Clean up allocated memory
	htable_destroy(&shared_htable);
	free(threads);
	free(thread_data);

	return 0;
}
