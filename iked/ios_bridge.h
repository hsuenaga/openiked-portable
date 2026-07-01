#ifndef __IOS_BRIDGE_H__
#define __IOS_BRIDGE_H__
#include <stdbool.h>
#include <stdarg.h>

typedef bool (^putsHandler)(const char * string);
typedef int (^vprintfHandler)(const char * fmt, va_list ap);
typedef void (^errorHandler)(int num, const char * string);

struct iked_bridge {
	struct iked *iked_env;

	putsHandler iked_puts;
	vprintfHandler iked_vprintf;
	errorHandler iked_error;
};

extern struct iked_bridge *iked_bridge;

/* XXX: TO BE REMOVED */
int ios_main(int argc, char *argv[]);

/* Swift API */
bool initIKE(vprintfHandler hnd_vp, putsHandler hnd_puts, errorHandler hnd_err);

#endif /* __IOS_BRIDGE_H__ */
