#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

/*********************************************************************
** Thread
*********************************************************************/

typedef struct abyss_thread TThread;

void
ThreadPoolInit(void);

typedef void TThreadProc(void * const userHandleP);
typedef void TThreadDoneFn(void * const userHandleP);

void
ThreadCreate(TThread **      const threadPP,
             void *          const userHandle,
             TThreadProc   * const func,
             TThreadDoneFn * const threadDone,
             abyss_bool      const useSigchld,
             const char **   const errorP);

abyss_bool
ThreadRun(TThread * const threadP);

abyss_bool
ThreadStop(TThread * const threadP);

abyss_bool
ThreadKill(TThread * threadP);

void
ThreadWaitAndRelease(TThread * const threadP);

void
ThreadExit(int const retValue);

void
ThreadRelease(TThread * const threadP);

abyss_bool
ThreadForks(void);

void
ThreadUpdateStatus(TThread * const threadP);

#ifndef WIN32
void
ThreadHandleSigchld(pid_t const pid);
#endif

/*********************************************************************
** Mutex
*********************************************************************/

#ifdef WIN32
typedef HANDLE TMutex;
#else
#include <pthread.h>
typedef pthread_mutex_t TMutex;
#endif  /* WIN32 */

abyss_bool MutexCreate(TMutex *m);
abyss_bool MutexLock(TMutex *m);
abyss_bool MutexUnlock(TMutex *m);
abyss_bool MutexTryLock(TMutex *m);
void MutexFree(TMutex *m);

#endif
