#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#include "rr_bgtask.h"
#include "rr_logging.h"
#include "rr_malloc.h"
#include "adlist.h"

#define THREAD_STACK_SIZE (4 * 1024 * 1024)

struct Task {
    int sub_type;
    void *arg;
};

static pthread_t task_runners[TASK_NTYPES];
static pthread_mutex_t locks[TASK_NTYPES];
static pthread_cond_t cons[TASK_NTYPES];
static list *tasks[TASK_NTYPES];

static void *thread_handler(void *arg);

void rr_bgt_init(void) {
    int i;
    pthread_attr_t attr;
    size_t stacksize;

    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr, &stacksize);
    if (!stacksize) stacksize = 1;
    while (stacksize < THREAD_STACK_SIZE) stacksize *= 2;
    pthread_attr_setstacksize(&attr, stacksize);

    for (i = 0; i < TASK_NTYPES; i++) {
        if (pthread_create(task_runners+i, &attr, thread_handler,
            (void *)(unsigned long) i) != 0)
        {
            rr_log(RR_LOG_CRITICAL, "Can not create thread for the background tasks");
            exit(1);
        }
        pthread_mutex_init(locks+i, NULL);
        pthread_cond_init(cons+i, NULL);
        tasks[i] = listCreate();
    }
}

static void *thread_handler(void *arg) {
    int type = (int) arg;
    struct Task *task;

    sigset_t sigset;

    if (type >= TASK_NTYPES) {
        rr_log(RR_LOG_ERROR,
               "Error: background thread started with wrong type %d", type);
        return NULL;
    }

    /* Make the thread killable at any time, so that bioKillThreads()
     * can work reliably. */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    pthread_mutex_lock(locks+type);
    /* Block SIGALRM so we are sure that only the main thread will
     * receive the watchdog signal. */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL)) {
        rr_log(RR_LOG_WARNING,
               "Warning: can't mask SIGALRM in background thread: %s",
               strerror(errno));
    }

    for(;;) {
        listNode *ln;

        /* The loop always starts with the lock hold. */
        if (listLength(tasks[type]) == 0) {
            pthread_cond_wait(cons+type, locks+type);
            continue;
        }
        /* Pop the job from the queue. */
        ln = listFirst(tasks[type]);
        task = ln->value;
        /* It is now possible to unlock the background system as we know have
         * a stand alone job structure to process.*/
        pthread_mutex_unlock(locks+type);

        /* Process the job based on its type. */
        if (type == TASK_LAZY_FREE) {
            /* TODO: add lazy free dispatcher here */
            rr_log(RR_LOG_INFO, "Echoing from background thread %d", type);
        }
        rr_free(task);

        /* Lock again before reiterating the loop, if there are no longer
         * jobs to process we'll block again in pthread_cond_wait(). */
        pthread_mutex_lock(locks+type);
        listDelNode(tasks[type], ln);
    }

    return NULL;
}

void rr_bgt_add_task(int type, int sub_type, void *ud) {
    assert(type < TASK_NTYPES);
    struct Task *task = rr_malloc(sizeof(*task));
    task->sub_type = sub_type;
    task->arg = ud;

    pthread_mutex_lock(locks+type);
    listAddNodeTail(tasks[type], task);
    pthread_cond_signal(cons+type);
    pthread_mutex_unlock(locks+type);
}

void rr_bgt_terminate(void) {
    int err, i;

    for (i = 0; i < TASK_NTYPES; i++) {
        if (pthread_cancel(task_runners[i]) == 0) {
            if ((err = pthread_join(task_runners[i], NULL)) != 0) {
                rr_log(RR_LOG_WARNING,
                       "Background thread type #%d can not be joined: %s",
                       i, strerror(err));
            } else {
                rr_log(RR_LOG_WARNING, "Background thread type #%d terminated", i);
            }
        }
    }
}
