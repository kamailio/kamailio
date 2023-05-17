#include <pthread.h>
#include <dlfcn.h>

#define SYMBOL_EXPORT __attribute__((visibility("default")))

int SYMBOL_EXPORT pthread_mutex_init(
		pthread_mutex_t *__mutex, const pthread_mutexattr_t *__mutexattr)
{
	static int (*real_pthread_mutex_init)(
			pthread_mutex_t * __mutex, const pthread_mutexattr_t *__mutexattr);
	if(!real_pthread_mutex_init)
		real_pthread_mutex_init = dlsym(RTLD_NEXT, "pthread_mutex_init");

	if(__mutexattr) {
		pthread_mutexattr_t attr = *__mutexattr;
		pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
		return real_pthread_mutex_init(__mutex, &attr);
	}

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	int ret = real_pthread_mutex_init(__mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	return ret;
}

int SYMBOL_EXPORT pthread_rwlock_init(pthread_rwlock_t *__restrict __rwlock,
		const pthread_rwlockattr_t *__restrict __attr)
{
	static int (*real_pthread_rwlock_init)(
			pthread_rwlock_t *__restrict __rwlock,
			const pthread_rwlockattr_t *__restrict __attr);
	if(!real_pthread_rwlock_init)
		real_pthread_rwlock_init = dlsym(RTLD_NEXT, "pthread_rwlock_init");

	if(__attr) {
		pthread_rwlockattr_t attr = *__attr;
		pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
		return real_pthread_rwlock_init(__rwlock, &attr);
	}

	pthread_rwlockattr_t attr;
	pthread_rwlockattr_init(&attr);
	pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	int ret = real_pthread_rwlock_init(__rwlock, &attr);
	pthread_rwlockattr_destroy(&attr);
	return ret;
}
