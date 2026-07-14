/*	$OpenBSD: iked.c,v 1.72 2024/12/26 18:24:54 sthen Exp $	*/

/*
 * Copyright (c) 2019 Tobias Heider <tobias.heider@stusta.de>
 * Copyright (c) 2010-2013 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/uio.h>
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <Block.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <err.h>
#include <pwd.h>
#include <event.h>
#include <event2/thread.h>
#include <pthread.h>
#include <assert.h>

#include "iked.h"
#include "version.h"
#include "swift_bridge.h"
#include "swift_bridge_internal.h"

#ifdef WITH_APPARMOR
#include "apparmor.h"
#endif

/*
 * Task definitions
 */
void	 parent_shutdown(struct iked *);
void	 parent_sig_handler(int, short, void *);
int	 parent_dispatch_ca(int, struct privsep_proc *, struct imsg *);
int	 parent_dispatch_control(int, struct privsep_proc *, struct imsg *);
int	 parent_dispatch_ikev2(int, struct privsep_proc *, struct imsg *);
void	 parent_connected(struct privsep *);
int	 parent_configure(struct iked *);
static struct privsep_proc procs[] = {
	{ "ca",		PROC_CERT,	parent_dispatch_ca, caproc, IKED_CA },
	{ "control",	PROC_CONTROL,	parent_dispatch_control, control },
	{ "ikev2",	PROC_IKEV2,	parent_dispatch_ikev2, ikev2 }
};

/*
 * Thread management.
 */
struct iked_thread_hd iked_threads = LIST_HEAD_INITIALIZER(iked_thread_hd);
pthread_mutex_t iked_threads_lock = PTHREAD_MUTEX_INITIALIZER;
static int iked_thread_idx = 0;
static int iked_parent_idx = -1;

/*
 * Closures
 */
putsHandler hnd_puts = NULL;
errorHandler hnd_err = NULL;

/*
 * Globally shared data
 */
OpenIKEDConfig *swift_cf;
struct swift_bridge_internal swi = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.console_lock = PTHREAD_MUTEX_INITIALIZER
};
struct global_env envLock = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.owner = 0,
	.isHeld = false
};

/*
 * Instance of TLS
 */
__thread struct iked *iked_env = NULL;
__thread int parent_sock_fileno = -1;
__thread struct event_base *iked_ev_base = NULL;
__thread bool tear_down = false;

void
initTLS(void) {
	iked_env = NULL;
	parent_sock_fileno = -1;
	iked_ev_base = NULL;
	tear_down = false;
}

/*
 * Global locks
 */
void lockGlobal(void) {
	pthread_mutex_lock(&swi.lock);
}

void unlockGlobal(void) {
	pthread_mutex_unlock(&swi.lock);
}

void lockConsole(void) {
	pthread_mutex_lock(&swi.console_lock);
}

void unlockConsole(void) {
	pthread_mutex_unlock(&swi.console_lock);
}

struct iked *
retainEnv(void)
{
	assert(envLock.isHeld == false || envLock.owner != pthread_self());

	pthread_mutex_lock(&envLock.lock);
	envLock.isHeld = true;
	envLock.owner = pthread_self();
	return envLock.env;
}

void
releaseEnv(void)
{
	assert(envLock.isHeld == true && envLock.owner == pthread_self());

	envLock.isHeld = false;
	envLock.owner = 0;
	pthread_mutex_unlock(&envLock.lock);
}

struct iked *
copyEnv(const char *title)
{
	struct iked *env;
	struct iked *newEnv = NULL;
	struct privsep *ps = NULL;
	int proc_id;
	int proc_instance = 0;

	if (title == NULL)
		return NULL;

	proc_id = proc_getid(procs, nitems(procs), title);
	if (proc_id == PROC_MAX) {
		swift_error(1, "copyEnv: invalid title");
		return NULL;
	}

	env = retainEnv();
	if (env == NULL) {
		releaseEnv();
		return NULL;
	}

	// iked_env lock held
	newEnv = calloc(1, sizeof(*newEnv));
	if (newEnv == NULL) {
		goto done;
	}

	memcpy(newEnv, env, sizeof(*newEnv));
	// detach external objects.
	newEnv->sc_defaultcon = NULL;
	newEnv->sc_priv = NULL;	
	newEnv->sc_certreq = NULL;
	newEnv->sc_vroute = NULL;
	newEnv->sc_sock4[0] = NULL;
	newEnv->sc_sock4[1] = NULL;
	newEnv->sc_sock6[0] = NULL;
	newEnv->sc_sock6[1] = NULL;
	newEnv->sc_ocsp_url = NULL;
	// construct individual privsep structure.
	ps = &newEnv->sc_ps;
	memset(ps, 0, sizeof(*ps));
	ps->ps_env = newEnv;
	ps->ps_pw = env->sc_ps.ps_pw;
	ps->ps_csock.cs_name = env->sc_ps.ps_csock.cs_name;
	ps->ps_noaction = env->sc_ps.ps_noaction;
	ps->ps_instance = proc_instance;
	ps->ps_title[proc_id] = title;
	// further initialization will be done by proc_init().

done:
	releaseEnv();
	return newEnv;
}

/*
 * Console handling
 */
bool
swift_puts(const char *string)
{
	bool ret;

	lockConsole();
	if (hnd_puts) {
		ret = hnd_puts(string);
	}
	else {
		fprintf(stderr, "(swift_puts): %s", string);
	}
	unlockConsole();
	return ret;
}

void
swift_error(int num, const char *message)
{
	lockConsole();	
	if (hnd_err) {
		hnd_err(num, message);
	}
	else {
		fprintf(stderr, "(swift_error): %d: %s", num, message);
	}
	unlockConsole();
	return;
}

int
swift_vprintf(const char *fmt, va_list ap)
{
	int ret;
	char *msg = NULL;

	ret = vasprintf(&msg, fmt, ap);
	if (msg) {
		swift_puts(msg);
		free(msg);
	}
	return ret;
}

int
swift_printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = swift_vprintf(fmt, ap);
	va_end(ap);

	return ret;
}

void
swift_vlog(int priority, const char *message, va_list ap)
{
	(void)swift_vprintf(message, ap);
}

static char *
strdup_d(const char *src, const char *def)
{
	if (src == NULL) {
		return (char *)strdup(def);
	}

	return (char *)strdup(src);	
}

int
detach_thread(void*(func(void *)), void *arg)
{
	struct iked_thread_list *tl;
	int r;

	tl = calloc(1, sizeof(*tl));
	if (tl == NULL) {
		return -1;
	}

	pthread_mutex_lock(&iked_threads_lock);
	tl->idx = iked_thread_idx++;
	tl->finished = false;
	r = pthread_create(&tl->tid, NULL, func, arg);
	if (r != 0) {
		swift_printf("pthread_create() failed: %s",
		    strerror(errno));
		free(tl);
		pthread_mutex_unlock(&iked_threads_lock);
		return -1;
	}
	LIST_INSERT_HEAD(&iked_threads, tl, next);

	swift_printf("Detach thread(0x%p): idx %d",
	    (void *)tl->tid, tl->idx);
	r = tl->idx;
	pthread_mutex_unlock(&iked_threads_lock);
	
	return r;
}

void
join_all_threads(void)
{
	struct iked_thread_list *tl;
	pthread_t tid;
	bool found;
	int idx, r;

	for (;;) {
		found = false;
		pthread_mutex_lock(&iked_threads_lock);
		LIST_FOREACH(tl, &iked_threads, next) {
			if (tl->finished || tl->tid == pthread_self())
				continue;
			tl->finished = true;
			found = true;
			tid = tl->tid;
			idx = tl->idx;
			break;
		}
		pthread_mutex_unlock(&iked_threads_lock);
		if (!found)
			break;

		r = pthread_join(tid, NULL);
		if (r != 0) {
			swift_printf("pthread_join(0x%p) failed: %d(%s)",
			(void*)tid, r, strerror(r));
		}
		else {
			swift_printf("Join thread(0x%p): idx %d",
			(void *)tid, idx);
		}
	}
}

void
join_thread(int idx)
{
	struct iked_thread_list *tl;
	pthread_t tid;
	bool found = false;
	int r;

	pthread_mutex_lock(&iked_threads_lock);
	LIST_FOREACH(tl, &iked_threads, next) {
		if (tl->idx != idx || tl->finished || tl->tid == pthread_self())
			continue;
		tl->finished = true;
		tid = tl->tid;
		found = true;
		break;
	}
	pthread_mutex_unlock(&iked_threads_lock);
	if (!found) {
		swift_printf("Cannot find specified thread(%d)", idx);
		return;
	}

	r = pthread_join(tid, NULL);
	if (r != 0) {
		swift_printf("pthread_join(0x%p) failed: %d(%s)",
		    (void *)tid, r, strerror(r));
		return;
	}
	else {
		swift_printf("Join thread(0x%p): idx %d",
		    (void *)tid, idx);
	}
}

void
clear_thread(void)
{
	struct iked_thread_list *tl;

	// must be called after all threads are joined.
	pthread_mutex_lock(&iked_threads_lock);
	while (!LIST_EMPTY(&iked_threads)) {
		tl = LIST_FIRST(&iked_threads);
		assert(tl->finished);
		LIST_REMOVE(tl, next);
		free(tl);
	}
	pthread_mutex_unlock(&iked_threads_lock);
}

int
gettidx(void)
{
	struct iked_thread_list *tl = NULL;

	pthread_mutex_lock(&iked_threads_lock);
	LIST_FOREACH(tl, &iked_threads, next) {
		if (tl->tid != pthread_self())
			continue;
		break;
	}
	pthread_mutex_unlock(&iked_threads_lock);

	return tl ? tl->idx : -1;
}

/*
 * called from swift. don't forget ARC.
 */
bool
initIKE(const OpenIKEDConfig *cf,
    putsHandler sw_puts, errorHandler sw_err)
{
	struct iked *env = NULL;

	//
	// initialize libevent.
	//
	evthread_use_pthreads();

	//
	//  br is swift object, so it will be released automatically.
	//  we need to create local copy.
	//
	//  NOTE: closures are managed by ARC(-fblocks), those are
	//        retained carefully. don't use anonymous memcpy(3)
	//        to ensure ARC.
	//
	hnd_puts = Block_copy(sw_puts);
	hnd_err = Block_copy(sw_err);

	swift_cf = calloc(1, sizeof(*swift_cf));
	if (swift_cf == NULL) {
		printf("failed to alloc bridge structure.\n");
		return false;
	}
	if (cf->port == 0) {
		swift_cf->port = IKED_NATT_PORT;
	}
	swift_cf->configurationFile =
		strdup_d(cf->configurationFile, IKED_CONFIG);
	swift_cf->controlSocket =
		strdup_d(cf->controlSocket, IKED_SOCKET);	
	swift_cf->ikedPrivKey =
		strdup_d(cf->ikedPrivKey, IKED_PRIVKEY);
	swift_cf->ikedCADir =
		strdup_d(cf->ikedCADir, IKED_CA_DIR);
	swift_cf->ikedCRLDir =
		strdup_d(cf->ikedCRLDir, IKED_CRL_DIR);
	swift_cf->ikedCertDir =
		strdup_d(cf->ikedCertDir, IKED_CERT_DIR);
	swift_cf->resourcePath =
		strdup_d(cf->resourcePath, "/");

	swift_cf->debug = cf->debug;
	swift_cf->verbose = cf->verbose;
	swift_cf->procInstance = cf->procInstance;
	swift_cf->opts = cf->opts;

	if (swift_cf->procInstance != 0) {
		printf("[initIKE] WARNING: multiple instance is not supported yet.");
	}

	printf("[initIKE] using Configuration: %s\n", swift_cf->configurationFile);
	printf("[initIKE] using Control Socket: %s\n", swift_cf->controlSocket);
	printf("[initIKE] using Private Key: %s\n", swift_cf->ikedPrivKey);
	printf("[initIKE] using CADir: %s\n", swift_cf->ikedCADir);
	printf("[initIKE] using CRLDir: %s\n", swift_cf->ikedCRLDir);
	printf("[initIKE] using CertDir: %s\n", swift_cf->ikedCertDir);
	printf("[initIKE] using App. Bundle Resource: %s\n", swift_cf->resourcePath);	

	//
	//  logging hack. see log.c.
	//  will be removed in the future.
	//
	log_ext = swift_vlog;

	//
	//  create and initialize new environment.
	//  This environment is a kind of singleton at this time.
	//  We cannot handle multiple instances at now.
	//
	env = retainEnv();
	if (env != NULL) {
		swift_error(1, "[initIKE] already initialized.");
		releaseEnv();
		goto bailout;
	}
	if ((env = calloc(1, sizeof(*env))) == NULL) {
		swift_error(1, "calloc: ctx");
		releaseEnv();
		goto bailout;
	}
	envLock.env = env;
	releaseEnv();

	swift_puts("[initIKE] iked initialization complete.");
	return true;

bailout:
	if (env) {
		retainEnv();
		if (envLock.env == env) {
			envLock.env = NULL;
		}
		releaseEnv();
		free(env);
	}

	deinitIKE();
	return false;
}

void
deinitIKE(void)
{
	struct iked *env;

	if (swift_cf) {
		// tell ARC to release closures.
		Block_release(hnd_puts); hnd_puts = NULL;
		Block_release(hnd_err); hnd_err = NULL;
		
		// free manually allocated data.
		if (swift_cf->configurationFile) {
			free(swift_cf->configurationFile);
		}
		if (swift_cf->controlSocket) {
			free(swift_cf->controlSocket);
		}
		if (swift_cf->ikedPrivKey) {
			free(swift_cf->ikedPrivKey);
		}
		if (swift_cf->ikedCADir) {
			free(swift_cf->ikedCADir);
		}
		if (swift_cf->ikedCRLDir) {
			free(swift_cf->ikedCRLDir);
		}
		if (swift_cf->ikedCertDir) {
			free(swift_cf->ikedCertDir);
		}
		if (swift_cf->resourcePath) {
			free(swift_cf->resourcePath);
		}
		free(swift_cf);
		swift_cf = NULL;
	}
	env = retainEnv();
	if (env) {
		free(envLock.env);
		envLock.env = NULL;
	}
	releaseEnv();
	if (iked_ev_base) {
		event_base_free(iked_ev_base);
		iked_ev_base = NULL;
	}
	// WARNING: Not all thread local storages are freed here.
}

bool
addSymbol(char *definition)
{
	if (swift_cf == NULL)
		return false;
	if (cmdline_symset(definition) < 0)
		return false;

	return true;
}

// Parent thread
void *
ike_main(void *arg)
{
	int			 c;
	const char		*errstr;
	struct privsep		*ps;
	enum privsep_procid	 proc_id = PROC_PARENT;

	swift_printf("Starting IKEv2 thread %s (pid: %d, tidx: %d)\n",
	    IKED_VERSION, getpid(), gettidx());

	initTLS();
	
	if (swift_cf == NULL) {
		swift_printf("Error: swift_bridge is NULL\n");
		return false;
	}

	/* log to stderr until daemonized */
	log_init(swift_cf->debug ? swift_cf->debug : 1, LOG_DAEMON);

	/*
	 *  parent thread uses globally shared environment
	 *  as its own(thread local) environment.
	 */
	iked_env = retainEnv();
	if (iked_env == NULL) {
		swift_printf("Error: iked_env is NULL\n");
		return false;
	}
	iked_env->sc_opts = swift_cf->opts;
	iked_env->sc_nattmode = NATT_FORCE;
	iked_env->sc_nattport = swift_cf->port;
	iked_env->sc_path.privkey_file = swift_cf->ikedPrivKey;
	iked_env->sc_path.ca_dir = swift_cf->ikedCADir;
	iked_env->sc_path.crl_dir = swift_cf->ikedCRLDir;
	iked_env->sc_path.cert_dir = swift_cf->ikedCertDir;
	
	ps = &iked_env->sc_ps;
	ps->ps_env = iked_env;

	if (strlcpy(iked_env->sc_conffile, swift_cf->configurationFile,
	     PATH_MAX) >= PATH_MAX) {
		errx(1, "config file exceeds PATH_MAX");
	}

	/*
	 * Configure modules
	 * XXX: OK for other threads?
	 */
	ca_sslinit();
	group_init();
	policy_init(iked_env);

	ps->ps_csock.cs_name = swift_cf->controlSocket;

	log_init(swift_cf->debug, LOG_DAEMON);
	log_setverbose(swift_cf->verbose);

	if (swift_cf->opts & IKED_OPT_NOACTION)
		ps->ps_noaction = 1;

	ps->ps_instance = swift_cf->procInstance;
	ps->ps_title[proc_id] = NULL;
	releaseEnv();

	/*
	 * Environment setup is completed.
	 * Initialize workers now.
	 */
	swift_printf("[parent] Starting worker threads...\n");
	proc_init(ps, procs, nitems(procs), swift_cf->debug, 0, NULL, proc_id);
	setproctitle("parent");
	log_procinit("parent");

	iked_ev_base = event_base_new();
	if (iked_ev_base == NULL) {
		swift_printf("[parent] failed to initialize event base.\n");
		return NULL;
	}

	ps->ps_evsigint = evsignal_new(iked_ev_base, SIGINT, parent_sig_handler, ps);
	ps->ps_evsigterm = evsignal_new(iked_ev_base, SIGTERM, parent_sig_handler, ps);
	ps->ps_evsigchld = evsignal_new(iked_ev_base, SIGCHLD, parent_sig_handler, ps);
	ps->ps_evsighup = evsignal_new(iked_ev_base, SIGHUP, parent_sig_handler, ps);
	ps->ps_evsigpipe = evsignal_new(iked_ev_base, SIGPIPE, parent_sig_handler, ps);
	ps->ps_evsigusr1 = evsignal_new(iked_ev_base, SIGUSR1, parent_sig_handler, ps);

	evsignal_add(ps->ps_evsigint, NULL);
	evsignal_add(ps->ps_evsigterm, NULL);
	evsignal_add(ps->ps_evsigchld, NULL);
	evsignal_add(ps->ps_evsighup, NULL);
	evsignal_add(ps->ps_evsigpipe, NULL);
	evsignal_add(ps->ps_evsigusr1, NULL);

#if defined(HAVE_VROUTE)
	vroute_init(iked_env);
#endif

	swift_printf("[parent] proc_connect...\n");
	proc_connect(ps, parent_connected);

	swift_printf("[parent] dispatching events...\n");
	event_base_dispatch(iked_ev_base);

	swift_printf("[parent] exiting...\n");
	log_debug("[parent] pid %d, tidx %d, parent exiting", getpid(), gettidx());
	event_free(ps->ps_evsigint); ps->ps_evsigint = NULL;
	event_free(ps->ps_evsigterm); ps->ps_evsigterm = NULL;
	event_free(ps->ps_evsigchld); ps->ps_evsigchld = NULL;
	event_free(ps->ps_evsighup); ps->ps_evsighup = NULL;
	event_free(ps->ps_evsigpipe); ps->ps_evsigpipe = NULL;
	event_free(ps->ps_evsigusr1); ps->ps_evsigusr1 = NULL;

//	parent_shutdown(iked_env);
	event_base_free(iked_ev_base);
	iked_ev_base = NULL;

	retainEnv();
	assert(envLock.env == iked_env);
	free(iked_env);
	envLock.env = iked_env = NULL;	
	releaseEnv();

	swift_printf("[parent] waiting child threads.");
	join_all_threads();
	swift_printf("[parent] Finished");

	return NULL;
}

bool
startIKE(void)
{
	pthread_t thread;
	sigset_t sigmask, sigmask_old;;
	int ret;

	swift_printf("Starting IKE in a new thread...\n");

	// apply default mask (ALL BLOCK)
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGCHLD);
	sigaddset(&sigmask, SIGHUP);
	sigaddset(&sigmask, SIGPIPE);
	sigaddset(&sigmask, SIGUSR1);
	if (pthread_sigmask(SIG_BLOCK, &sigmask, &sigmask_old) != 0) {
		swift_error(1, "pthread_sigmask failed.");
		return false;
	}

	// detach main thread(parent)
	iked_parent_idx = detach_thread(ike_main, NULL);

	return iked_parent_idx < 0 ? false : true;
}

void
stopIKE(void)
{
	if (iked_parent_idx < 0)
		return;

	join_thread(iked_parent_idx);
	iked_parent_idx = -1;

	//assert(LIST_EMPTY(&iked_threads));
}

void
parent_connected(struct privsep *ps)
{
	struct iked	*env = ps->ps_env;	

	retainEnv();
	if (parent_configure(env) == -1)
		fatalx("configuration failed");
	releaseEnv();
}

int
parent_configure(struct iked *env)
{
	struct sockaddr_storage	 ss;

	if (parse_config(env->sc_conffile, env) == -1) {
		proc_kill(&env->sc_ps);
		exit(1);
	}

	if (env->sc_opts & IKED_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		proc_kill(&env->sc_ps);
		exit(0);
	}

	env->sc_pfkey = -1;
	config_setpfkey(env);

	/* Send private and public keys to cert after forking the children */
	if (config_setkeys(env) == -1)
		fatalx("%s: failed to send keys", __func__);
	config_setreset(env, RESET_CA, PROC_CERT);

	/* Now compile the policies and calculate skip steps */
	config_setcompile(env, PROC_IKEV2);

	bzero(&ss, sizeof(ss));
	ss.ss_family = AF_INET;

#ifdef __APPLE__
	int nattport = env->sc_nattport;
	sysctlbyname("net.inet.ipsec.esp_port", NULL, NULL, &nattport, sizeof(nattport));
#endif

	/* see comment on config_setsocket() */
	if (env->sc_nattmode != NATT_FORCE)
		config_setsocket(env, &ss, htons(IKED_IKE_PORT), PROC_IKEV2, 0);
	if (env->sc_nattmode != NATT_DISABLE)
		config_setsocket(env, &ss, htons(env->sc_nattport), PROC_IKEV2, 1);

	bzero(&ss, sizeof(ss));
	ss.ss_family = AF_INET6;

	if (env->sc_nattmode != NATT_FORCE)
		config_setsocket(env, &ss, htons(IKED_IKE_PORT), PROC_IKEV2, 0);
	if (env->sc_nattmode != NATT_DISABLE)
		config_setsocket(env, &ss, htons(env->sc_nattport), PROC_IKEV2, 1);

	/*
	 * pledge in the parent process:
	 * It has to run fairly late to allow forking the processes and
	 * opening the PFKEY socket and the listening UDP sockets (once)
	 * that need the bypass ioctls that are never allowed by pledge.
	 *
	 * Other flags:
	 * stdio - for malloc and basic I/O including events.
	 * rpath - for reload to open and read the configuration files.
	 * proc - run kill to terminate its children safely.
	 * dns - for reload and ocsp connect.
	 * inet - for ocsp connect.
	 * route - for using interfaces in iked.conf (SIOCGIFGMEMB)
	 * wroute - for adding and removing addresses (SIOCAIFGMEMB)
	 * sendfd - for ocsp sockets.
	 */
	if (pledge("stdio rpath proc dns inet route wroute sendfd", NULL) == -1)
		fatal("pledge");

	config_setstatic(env);
	config_setcoupled(env, env->sc_decoupled ? 0 : 1);
	config_setocsp(env);
	/* Must be last */
	config_setmode(env, env->sc_passive ? 1 : 0);

	return (0);
}

void
parent_reload(struct iked *env, int reset, const char *filename)
{
	/* Switch back to the default config file */
	if (filename == NULL || *filename == '\0')
		filename = env->sc_conffile;

	log_debug("%s: level %d config file %s", __func__, reset, filename);

	if (reset == RESET_RELOAD) {
		config_setreset(env, RESET_POLICY, PROC_IKEV2);
		if (config_setkeys(env) == -1)
			fatalx("%s: failed to send keys", __func__);
		config_setreset(env, RESET_CA, PROC_CERT);

		if (parse_config(filename, env) == -1) {
			log_debug("%s: failed to load config file %s",
			    __func__, filename);
		}

		/* Re-compile policies and skip steps */
		config_setcompile(env, PROC_IKEV2);

		config_setstatic(env);
		config_setcoupled(env, env->sc_decoupled ? 0 : 1);
		config_setocsp(env);
		/* Must be last */
		config_setmode(env, env->sc_passive ? 1 : 0);
	} else {
		config_setreset(env, reset, PROC_IKEV2);
		config_setreset(env, reset, PROC_CERT);
	}
}

void
parent_sig_handler(int sig, short event, void *arg)
{
	struct privsep	*ps = arg;
	int		 die = 0, status, fail, id;
	pid_t		 pid;
	char		*cause;

	switch (sig) {
	case SIGHUP:
		log_info("%s: reload requested with SIGHUP", __func__);

		/*
		 * This is safe because libevent uses async signal handlers
		 * that run in the event loop and not in signal context.
		 */
		retainEnv();
		parent_reload(ps->ps_env, 0, NULL);
		releaseEnv();
		break;
	case SIGPIPE:
		log_info("%s: ignoring SIGPIPE", __func__);
		break;
	case SIGUSR1:
		log_info("%s: ignoring SIGUSR1", __func__);
		break;
	case SIGTERM:
	case SIGINT:
		if (!tear_down) {
			tear_down = true;
			retainEnv();
			parent_shutdown(ps->ps_env);
			releaseEnv();
		}
		break;
	case SIGCHLD:
		fatalx("unexpected cause of SIGCHLD");
		break;
	default:
		fatalx("unexpected signal");
	}
}

int
parent_dispatch_ca(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct iked	*env = retainEnv();

	switch (imsg->hdr.type) {
	case IMSG_CTL_ACTIVE:
	case IMSG_CTL_PASSIVE:
		proc_forward_imsg(&env->sc_ps, imsg, PROC_IKEV2, -1);
		break;
	case IMSG_OCSP_FD:
		ocsp_connect(env, imsg);
		break;
	default:
		releaseEnv();
		return (-1);
	}

	releaseEnv();
	return (0);
}

int
parent_dispatch_control(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct iked	*env = retainEnv();
	int		 v;
	char		*str = NULL;
	unsigned int	 type = imsg->hdr.type;

	switch (type) {
	case IMSG_CTL_RESET:
		IMSG_SIZE_CHECK(imsg, &v);
		memcpy(&v, imsg->data, sizeof(v));
		parent_reload(env, v, NULL);
		break;
	case IMSG_CTL_COUPLE:
	case IMSG_CTL_DECOUPLE:
	case IMSG_CTL_ACTIVE:
	case IMSG_CTL_PASSIVE:
		proc_compose(&env->sc_ps, PROC_IKEV2, type, NULL, 0);
		break;
	case IMSG_CTL_RELOAD:
		if (IMSG_DATA_SIZE(imsg) > 0)
			str = get_string(imsg->data, IMSG_DATA_SIZE(imsg));
		parent_reload(env, 0, str);
		free(str);
		break;
	case IMSG_CTL_VERBOSE:
		proc_forward_imsg(&env->sc_ps, imsg, PROC_IKEV2, -1);
		proc_forward_imsg(&env->sc_ps, imsg, PROC_CERT, -1);

		/* return 1 to let proc.c handle it locally */
		releaseEnv();
		return (1);
	default:
		releaseEnv();
		return (-1);
	}

	releaseEnv();
	return (0);
}

int
parent_dispatch_ikev2(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct iked	*env = retainEnv();

	switch (imsg->hdr.type) {
#if defined(HAVE_VROUTE)
	case IMSG_IF_ADDADDR:
	case IMSG_IF_DELADDR:
		return (vroute_getaddr(env, imsg));
	case IMSG_VDNS_ADD:
	case IMSG_VDNS_DEL:
		return (vroute_getdns(env, imsg));
	case IMSG_VROUTE_ADD:
	case IMSG_VROUTE_DEL:
		return (vroute_getroute(env, imsg));
	case IMSG_VROUTE_CLONE:
		return (vroute_getcloneroute(env, imsg));
#endif
	default:
		releaseEnv();
		return (-1);
	}

	releaseEnv();
	return (0);
}

void
parent_shutdown(struct iked *env)
{
	tear_down = true;
	proc_kill(&env->sc_ps);

#if defined(HAVE_VROUTE)
	vroute_cleanup(env);
	free(env->sc_vroute);
	env->sc_vroute = NULL;
#endif

	log_warnx("parent terminating");
}
