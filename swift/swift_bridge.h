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
 * { (_ num: CInt,  UnsafePointer<CChar>?) -> Void in ... }
 */
typedef void (^errorHandler)(int num, const char * string);

typedef struct {
	/* configuration */
	uint16_t	 port;
	char		*configurationFile;
	char		*controlSocket;
	char   		*ikedPrivKey;
	char		*ikedCADir;
	char		*ikedCRLDir;
	char		*ikedCertDir;
	char   		*resourcePath;
	int		 debug;
	int		 verbose;
	int		 procInstance;
	int		 opts;
} OpenIKEDConfig;

/* Swift API */
bool initIKE(const OpenIKEDConfig *, putsHandler, errorHandler);
void deinitIKE(void);
bool startIKE(void);

bool addSymbol(char *);

#endif /* __SWIFT_BRIDGE_H__ */
