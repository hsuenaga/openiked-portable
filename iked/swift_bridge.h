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

static inline bool
swift_puts(const char *string)
{
	if (swift_bridge == NULL)
		return false;
	if (swift_bridge->swift_puts == NULL)
		return false;

	return swift_bridge->swift_puts(string);
}

static inline int
swift_vprintf(const char *fmt, va_list ap)
{
	if (swift_bridge == NULL)
		return false;
	if (swift_bridge->swift_vprintf == NULL)
		return false;

	return swift_bridge->swift_vprintf(fmt, ap);
}

static inline int
swift_printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = swift_vprintf(fmt, ap);
	va_end(ap);

	return ret;
}

static inline void
swift_error(int num, const char *message)
{
	if (swift_bridge == NULL)
		return;
	if (swift_bridge->swift_error == NULL)
		return;

	(void) swift_bridge->swift_error(num, message);
	return;
}


/* XXX: TO BE REMOVED */
int swift_main(int argc, char *argv[]);

/* Swift API */
bool initIKE(vprintfHandler hnd_vp, putsHandler hnd_puts, errorHandler hnd_err);

#endif /* __SWIFT_BRIDGE_H__ */
