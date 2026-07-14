#ifndef __SWIFT_BRIDGE_INTERNAL_H__
#define __SWIFT_BRIDGE_INTERNAL_H__
#include <sys/queue.h>
#include <stdbool.h>
#include <pthread.h>

#include "swift_bridge.h"

#ifdef PROC_PARENT_SOCK_FILENO
#undef PROC_PARENT_SOCK_FILENO
#endif
#define PROC_PARENT_SOCK_FILENO parent_sock_fileno

struct swift_bridge_internal {
	pthread_mutex_t lock;
        pthread_mutex_t console_lock;
};

struct global_env {
	pthread_mutex_t lock;
	pthread_t owner;
	bool isHeld;
	struct iked	*env;
};

struct iked_thread_list {
	pthread_t tid;
        int idx;
        bool finished;
	LIST_ENTRY(iked_thread_list) next;
};
LIST_HEAD(iked_thread_hd, iked_thread_list);

extern putsHandler hnd_puts;
extern errorHandler hnd_err;

/* Internal API. no need to call from Swift? */
void lockGlobal(void);
void unlockGlobal(void);
void lockConsole(void);
void unlockConsole(void);
struct iked *retainEnv(void);
void releaseEnv(void);
struct iked *copyEnv(const char *title);
bool swift_puts(const char *string);
int swift_vprintf(const char *fmt, va_list ap);
int swift_printf(const char *fmt, ...);
void swift_error(int num, const char *message);

int detach_thread(void *(*)(void *), void *);
void join_thread(int);
void join_all_threads(void);
int gettidx();

extern OpenIKEDConfig *swift_cf;

/*
 * List of TLS.
 */
extern __thread struct iked *iked_env;
extern __thread int parent_sock_fileno;
extern __thread struct event_base *iked_ev_base;
extern __thread bool tear_down;
void initTLS(void);

#endif /* __SWIFT_BRIDGE_INTERNAL_H__ */
