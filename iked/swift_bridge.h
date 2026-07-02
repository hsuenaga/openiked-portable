#ifndef __SWIFT_BRIDGE_H__
#define __SWIFT_BRIDGE_H__
#include <stdbool.h>
#include <stdarg.h>

typedef bool (^putsHandler)(const char * string);
typedef int (^vprintfHandler)(const char * fmt, va_list ap);
typedef void (^errorHandler)(int num, const char * string);

struct swift_bridge {
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


/* XXX: TO BE REMOVED */
int swift_main(int argc, char *argv[]);

/* Swift API */
bool initIKE(vprintfHandler hnd_vp, putsHandler hnd_puts, errorHandler hnd_err);
void deinitIKE(void);
bool startIKE(int, char *[]);

bool addSymbol(const char *);

#endif /* __SWIFT_BRIDGE_H__ */
