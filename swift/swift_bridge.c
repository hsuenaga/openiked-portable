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
#include <pthread.h>

#include "iked.h"
#include "ikev2.h"
#include "version.h"
#include "swift_bridge.h"

#ifdef WITH_APPARMOR
#include "apparmor.h"
#endif

struct swift_bridge *swift_bridge;

struct global_env {
	pthread_mutex_t lock;
	bool isHeld;
	struct iked	*env;
} envLock = {
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.isHeld = false
};
__thread struct iked *iked_env;

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

	swift_bridge->swift_error(num, message);
	return;
}

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

struct iked *
retainEnv(void)
{
	pthread_mutex_lock(&envLock.lock);
	if (envLock.env == NULL) {
		pthread_mutex_unlock(&envLock.lock);
		return NULL;
	}
	envLock.isHeld = true;
	return envLock.env;
}

void
releaseEnv(void)
{
	envLock.isHeld = false;
	pthread_mutex_unlock(&envLock.lock);
}

struct iked *
copyEnv(void)
{
	struct iked *env;
	struct iked *newEnv = NULL;

	env = retainEnv();
	if (env == NULL) {
		goto done;
	}
	newEnv = calloc(1, sizeof(*newEnv));
	if (newEnv == NULL) {
		goto done;
	}
	memcpy(newEnv, env, sizeof(*newEnv));

	// Create own imsg buffers for the new environment
	for (int id = 0; id < PROC_MAX; id++) {
		if (env->sc_ps.ps_ievs[id] == NULL)
				continue;
		newEnv->sc_ps.ps_ievs[id] = calloc(env->sc_ps.ps_instances[id],
		    sizeof(struct imsgev));
		if (newEnv->sc_ps.ps_ievs[id] == NULL) {
			free(newEnv);
			newEnv = NULL;
			goto done;
		}
		for (int i = 0; i < env->sc_ps.ps_instances[id]; i++) {
			newEnv->sc_ps.ps_ievs[id][i].handler = env->sc_ps.ps_ievs[id][i].handler;
			newEnv->sc_ps.ps_ievs[id][i].events = env->sc_ps.ps_ievs[id][i].events;
			newEnv->sc_ps.ps_ievs[id][i].proc = env->sc_ps.ps_ievs[id][i].proc;
			newEnv->sc_ps.ps_ievs[id][i].data = &newEnv->sc_ps.ps_ievs[id][i];
		}
	}

	// Copy ps_pipes
	for (int src = 0; src < PROC_MAX; src++) {
		if (env->sc_ps.ps_pipes[src] == NULL)
				continue;
		// src array: [PROC_SOURCE][Instance]
		newEnv->sc_ps.ps_pipes[src] = calloc(env->sc_ps.ps_instances[src],
		    sizeof(struct privsep_pipes));
		if (newEnv->sc_ps.ps_pipes[src] == NULL) {
			free(newEnv);
			newEnv = NULL;
			goto done;
		}
		memcpy(newEnv->sc_ps.ps_pipes[src], env->sc_ps.ps_pipes[src],
		    env->sc_ps.ps_instances[src] * sizeof(struct privsep_pipes));

		// dst array: [PROC_DESTINATION][Instance]
		for (int i = 0; i < env->sc_ps.ps_instances[src]; i++) {
			struct privsep_pipes *pp_src, *pp_dst;
			pp_src = &env->sc_ps.ps_pipes[src][i];
			pp_dst = &newEnv->sc_ps.ps_pipes[src][i];

			for (int dst = 0; dst < PROC_MAX; dst++) {
				if (pp_src->pp_pipes[dst] == NULL)
					continue;
				pp_dst->pp_pipes[dst] = calloc(env->sc_ps.ps_instances[dst],
				    sizeof(int));
				if (pp_dst->pp_pipes[dst] == NULL) {
					free(newEnv);
					newEnv = NULL;
					goto done;
				}
				memcpy(pp_dst->pp_pipes[dst], pp_src->pp_pipes[dst],
				    env->sc_ps.ps_instances[dst] * sizeof(int));
			}
		}
	}

done:
	// XXX: release partially allocated resources if newEnv is NULL
	releaseEnv();
	return newEnv;
}

/* called from swift */
bool
initIKE(vprintfHandler hnd_vp, putsHandler hnd_puts, errorHandler hnd_err)
{
	struct swift_bridge *bridge;
	struct iked *env;

	if ((bridge = calloc(1, sizeof(*bridge))) == NULL) {
		if (hnd_err != NULL)
			hnd_err(1, "calloc: bridge");
		return false;
	}
	swift_bridge = bridge;
	bridge->swift_puts = hnd_puts;
	bridge->swift_vprintf = hnd_vp;
	bridge->swift_error = hnd_err;

	env = retainEnv();
	if (env != NULL) {
		swift_error(1, "initIKE: env already initialized");
		return false;
	}
	if ((env = calloc(1, sizeof(*env))) == NULL) {
		swift_error(1, "calloc: ctx");
		free(bridge);
		swift_bridge = bridge = NULL;
		releaseEnv();
		return false;
	}
	envLock.env = env;
	iked_env = env;
	releaseEnv();

	bridge->port = IKED_NATT_PORT;
	bridge->configurationFile = IKED_CONFIG;
	bridge->controlSocket = IKED_SOCKET;
	bridge->debug = 0;
	bridge->verbose = 0;
	bridge->procInstance = 0;
	bridge->opts = 0;

	swift_puts("iked initialization complete.");
	return true;
}

void
deinitIKE(void)
{
	if (swift_bridge == NULL)
		return;

	/* release swift closures. */
	swift_bridge->swift_puts = NULL;
	swift_bridge->swift_vprintf = NULL;
	swift_bridge->swift_error = NULL;
}

bool
addSymbol(char *definition)
{
	if (swift_bridge == NULL)
		return false;
	if (cmdline_symset(definition) < 0)
		return false;

	return true;
}


bool
startIKE(void)
{
	int			 c;
	int			 debug = swift_bridge->debug;
	int			 verbose = swift_bridge->verbose;
	int			 opts = swift_bridge->opts;
	enum natt_mode		 natt_mode = NATT_FORCE;
	in_port_t		 port = swift_bridge->port;
	const char		*conffile = swift_bridge->configurationFile;
	const char		*sock = swift_bridge->controlSocket;;
	const char		*errstr, *title = NULL;
	struct privsep		*ps;
	enum privsep_procid	 proc_id = PROC_PARENT;
	int			 proc_instance = swift_bridge->procInstance;

	/* log to stderr until daemonized */
	log_init(debug ? debug : 1, LOG_DAEMON);

	if (swift_bridge == NULL)
		return false;
	if (iked_env == NULL)
		return false;

	retainEnv();
	iked_env->sc_opts = opts;
	iked_env->sc_nattmode = natt_mode;
	iked_env->sc_nattport = port;

	ps = &iked_env->sc_ps;
	ps->ps_env = iked_env;

	if (strlcpy(iked_env->sc_conffile, conffile, PATH_MAX) >= PATH_MAX)
		errx(1, "config file exceeds PATH_MAX");

	ca_sslinit();
	group_init();
	policy_init(iked_env);

	if ((ps->ps_pw =  getpwnam(IKED_USER)) == NULL)
		errx(1, "unknown user %s", IKED_USER);

	/* Configure the control socket */
	ps->ps_csock.cs_name = sock;

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	if (opts & IKED_OPT_NOACTION)
		ps->ps_noaction = 1;

	ps->ps_instance = proc_instance;
	if (title != NULL)
		ps->ps_title[proc_id] = title;
	releaseEnv();

	/* only the parent returns */
	proc_init(ps, procs, nitems(procs), debug, 0, NULL, proc_id);

	setproctitle("parent");
	log_procinit("parent");

	event_init();

	signal_set(&ps->ps_evsigint, SIGINT, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigterm, SIGTERM, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigchld, SIGCHLD, parent_sig_handler, ps);
	signal_set(&ps->ps_evsighup, SIGHUP, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigpipe, SIGPIPE, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigusr1, SIGUSR1, parent_sig_handler, ps);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsigchld, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);
	signal_add(&ps->ps_evsigusr1, NULL);

#if defined(HAVE_VROUTE)
	vroute_init(iked_env);
#endif

	proc_connect(ps, parent_connected);

	event_dispatch();

	log_debug("%d parent exiting", getpid());
	parent_shutdown(iked_env);

	return (0);
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
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		do {
			int len = 0;

			pid = waitpid(-1, &status, WNOHANG);
			if (pid <= 0)
				continue;

			fail = 0;
			if (WIFSIGNALED(status)) {
				fail = 1;
				len = asprintf(&cause, "terminated; signal %d",
				    WTERMSIG(status));
			} else if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					fail = 1;
					len = asprintf(&cause,
					    "exited abnormally");
				} else {
					len = asprintf(&cause, "exited okay");
					break;
				}
			} else
				fatalx("unexpected cause of SIGCHLD");

			if (len == -1)
				fatal("asprintf");

			die = 1;

			for (id = 0; id < PROC_MAX; id++)
				if (pid == ps->ps_pid[id]) {
					if (fail)
						log_warnx("lost child: %s %s",
						    ps->ps_title[id], cause);
					break;
				}

			free(cause);
		} while (pid > 0 || (pid == -1 && errno == EINTR));

		if (die) {
			retainEnv();
			parent_shutdown(ps->ps_env);
			releaseEnv();
		}
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
	proc_kill(&env->sc_ps);

#if defined(HAVE_VROUTE)
	vroute_cleanup(env);
#endif
	free(env->sc_vroute);
	free(env);

	log_warnx("parent terminating");
	exit(0);
}