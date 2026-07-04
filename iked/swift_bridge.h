#ifndef __SWIFT_BRIDGE_H__
#define __SWIFT_BRIDGE_H__
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

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

bool addSymbol(char *);

#endif /* __SWIFT_BRIDGE_H__ */
