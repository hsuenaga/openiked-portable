#ifndef __SWIFT_BRIDGE_H__
#define __SWIFT_BRIDGE_H__
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <pthread.h>

#ifdef PROC_PARENT_SOCK_FILENO
#undef PROC_PARENT_SOCK_FILENO
#endif
#define PROC_PARENT_SOCK_FILENO parent_sock_fileno

/*
 * closure:
 * { (_ string: UnsafePointer<CChar>?) -> CBool in ... }
 */
typedef bool (^putsHandler)(const char * string);

/*
 * closure:
 * { (_ fmt: UnsafePointer<CChar>?, _ va: CVaListPointer?) -> CInt in ... }
 */
typedef int (^vprintfHandler)(const char * fmt, va_list ap);

/*
 * closure:
 * { (_ num: CInt,  UnsafePointer<CChar>?) -> Void in ... }
 */
typedef void (^errorHandler)(int num, const char * string);

struct swift_bridge {
	pthread_mutex_t lock;

	putsHandler swift_puts;
	vprintfHandler swift_vprintf;
	errorHandler swift_error;

	/* configuration */
	uint16_t	port;
	const char*	configurationFile;
	const char*	controlSocket;
	int		debug;
	int		verbose;
	int		procInstance;
	int		opts;
};

extern struct swift_bridge *swift_bridge;
extern __thread int parent_sock_fileno;


/* Internal API. no need to call from Swift? */
struct iked *retainEnv(void);
void releaseEnv(void);
struct iked *copyEnv(const char *title);
bool swift_puts(const char *string);
int swift_vprintf(const char *fmt, va_list ap);
int swift_printf(const char *fmt, ...);
void swift_error(int num, const char *message);

/* Swift API */
bool initIKE(vprintfHandler hnd_vp, putsHandler hnd_puts, errorHandler hnd_err);
void deinitIKE(void);
bool startIKE(void);

bool addSymbol(char *);

#endif /* __SWIFT_BRIDGE_H__ */
