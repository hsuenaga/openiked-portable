#ifndef __SWIFT_BRIDGE_H__
#define __SWIFT_BRIDGE_H__
#include <stdbool.h>
#include <stdarg.h>

typedef bool (^putsHandler)(const char * string);
typedef int (^vprintfHandler)(const char * fmt, va_list ap);
typedef void (^errorHandler)(int num, const char * string);

struct swift_bridge {
	struct iked *iked_env;

	putsHandler swift_puts;
	vprintfHandler swift_vprintf;
	errorHandler swift_error;
};

extern struct swift_bridge *swift_bridge;


/* XXX: TO BE REMOVED */
int swift_main(int argc, char *argv[]);

/* Swift API */
bool initIKE(vprintfHandler hnd_vp, putsHandler hnd_puts, errorHandler hnd_err);

#endif /* __SWIFT_BRIDGE_H__ */
