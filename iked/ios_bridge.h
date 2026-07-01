#ifndef __IOS_BRIDGE_H__
#define __IOS_BRIDGE_H__

#ifndef IOS_BRIDGE
#error "iOS build is not configured."
#endif

#include <stdbool.h>
#include <stdarg.h>

typedef bool (^putsHandler)(const char * string);
typedef int (^vprintfHandler)(const char * fmt, va_list ap);
typedef void (^errorHandler)(int num, const char * string);

struct ios_bridge {
	struct iked *iked_env;

	putsHandler ios_puts;
	vprintfHandler ios_vprintf;
	errorHandler ios_error;
};

extern struct ios_bridge *ios_bridge;

static inline bool
ios_puts(const char *string)
{
	if (ios_bridge == NULL)
		return false;
	if (ios_bridge->ios_puts == NULL)
		return false;

	return ios_bridge->ios_puts(string);
}

static inline int
ios_vprintf(const char *fmt, va_list ap)
{
	if (ios_bridge == NULL)
		return false;
	if (ios_bridge->ios_vprintf == NULL)
		return false;

	return ios_bridge->ios_vprintf(fmt, ap);
}

static inline int
ios_printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = ios_vprintf(fmt, ap);
	va_end(ap);

	return ret;
}

static inline void
ios_error(int num, const char *message)
{
	if (ios_bridge == NULL)
		return;
	if (ios_bridge->ios_error == NULL)
		return;

	(void) ios_bridge->ios_error(num, message);
	return;
}


/* XXX: TO BE REMOVED */
int ios_main(int argc, char *argv[]);

/* Swift API */
bool initIKE(vprintfHandler hnd_vp, putsHandler hnd_puts, errorHandler hnd_err);

#endif /* __IOS_BRIDGE_H__ */
