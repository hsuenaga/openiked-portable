if (DEFINED ENV{DESTDIR})
	# Refer cross compile environment.
	include_directories("$ENV{DESTDIR}/usr/include")
	link_directories("$ENV{DESTDIR}/usr/lib")
else()
	# Refer local environment.
	include_directories("/usr/local/include")
	link_directories("/usr/local/lib")
endif()

if (CMAKE_SYSTEM_NAME MATCHES "iOS")
	if (NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
		set (CMAKE_INSTALL_SYSCONFDIR ${CMAKE_INSTALL_PREFIX}/etc)
	endif()
	add_compile_definitions(
		IKED_CONFIG="${CMAKE_INSTALL_SYSCONFDIR}/iked.conf"
		IKED_CA="${CMAKE_INSTALL_SYSCONFDIR}/iked/"
		HAVE_APPLE_NATT
		HAVE_SOCKADDR_SA_LEN
		SWIFT_BRIDGE
	)
	if(THREAD)
		add_compile_definitions(
			THREAD
		)
	endif()
	set(HAVE_VROUTE OFF)
	# iOS always needs library version.
	set(BUILD_LIBRARY ON)
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
	if (NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
		set (CMAKE_INSTALL_SYSCONFDIR ${CMAKE_INSTALL_PREFIX}/etc)
	endif()
	add_compile_definitions(
		IKED_CONFIG="${CMAKE_INSTALL_SYSCONFDIR}/iked.conf"
		IKED_CA="${CMAKE_INSTALL_SYSCONFDIR}/iked/"
		HAVE_APPLE_NATT
		HAVE_SOCKADDR_SA_LEN
	)
	if (HOMEBREW AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
		include_directories("/opt/homebrew/include")
		link_directories("/opt/homebrew/lib")
		include_directories("/opt/homebrew/opt/openssl/include")
        	link_directories("/opt/homebrew/opt/openssl/lib")
	else()
		include_directories("/usr/local/opt/openssl/include")
		link_directories("/usr/local/opt/openssl/lib")
	endif()
	if(BUILD_LIBRARY AND THREAD)
		add_compile_definitions(
			THREAD
		)
	endif()
	if (NOT BUILD_LIBRARY)
		set(HAVE_VROUTE ON)
	endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
	if (NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
		set (CMAKE_INSTALL_SYSCONFDIR /etc)
	endif()
	add_compile_definitions(
		HAVE_ATTRIBUTE__BOUNDED__
		HAVE_ATTRIBUTE__DEAD__
		HAVE_SOCKADDR_SA_LEN
	)
	set(HAVE_VROUTE ON)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
	if (NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
		set (CMAKE_INSTALL_SYSCONFDIR ${CMAKE_INSTALL_PREFIX}/etc)
	endif()
	add_compile_definitions(
		KED_CONFIG="${CMAKE_INSTALL_SYSCONFDIR}/iked.conf"
		IKED_CA="${CMAKE_INSTALL_SYSCONFDIR}/iked/"
		HAVE_SOCKADDR_SA_LEN
	)
	set(HAVE_VROUTE ON)
elseif(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
	if (NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
		set (CMAKE_INSTALL_SYSCONFDIR /etc)
	endif()
	add_compile_definitions(
		HAVE_SOCKADDR_SA_LEN
		_OPENBSD_SOURCE
	)
	set(HAVE_VROUTE ON)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	if (NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
		set (CMAKE_INSTALL_SYSCONFDIR /etc)
	endif()
	if (NOT DEFINED CMAKE_INSTALL_MANDIR)
		set (CMAKE_INSTALL_MANDIR /usr/share/man)
	endif()
	add_compile_definitions(
		_GNU_SOURCE
		_DEFAULT_SOURCE
		HAVE_UDPENCAP6
		SPT_TYPE=SPT_REUSEARGV
	)
	set(HAVE_VROUTE_NETLINK ON)
endif()

if (NOT DEFINED CMAKE_INSTALL_MANDIR)
	set (CMAKE_INSTALL_MANDIR ${CMAKE_INSTALL_PREFIX}/man)
endif()
if (NOT DEFINED CMAKE_INSTALL_SBINDIR)
	set (CMAKE_INSTALL_SBINDIR ${CMAKE_INSTALL_PREFIX}/sbin)
endif()

if (NOT APPLE OR NOT CMAKE_GENERATOR MATCHES "Xcode")
check_linker_flag(C "LINKER:-z,now,-z,relro" HAVE_LD_Z)
endif()

check_include_files("sys/types.h;net/pfkeyv2.h" HAVE_NET_PFKEY_H)
if(HAVE_NET_PFKEY_H)
	add_compile_definitions(HAVE_NET_PFKEY_H)
endif()

check_include_files("linux/pfkeyv2.h" HAVE_LINUX_PFKEY_H)
if(HAVE_LINUX_PFKEY_H)
	add_compile_definitions(HAVE_LINUX_PFKEY_H)
endif()

check_include_files(unistd.h HAVE_UNISTD_H)
if(HAVE_UNISTD_H)
	add_compile_definitions(HAVE_UNISTD_H)
endif()

check_include_files(endian.h HAVE_ENDIAN_H)
if(HAVE_ENDIAN_H)
	add_compile_definitions(HAVE_ENDIAN_H)
endif()

check_include_files(dirent.h HAVE_DIRENT_H)
if(HAVE_DIRENT_H)
	add_compile_definitions(HAVE_DIRENT_H)
endif()

check_include_files(grp.h HAVE_GRP_H)
if(HAVE_GRP_H)
	add_compile_definitions(HAVE_GRP_H)
endif()

check_include_files("sys/socket.h;netinet/ip_ipsp.h" HAVE_IPSP_H)
if(HAVE_IPSP_H)
	add_compile_definitions(HAVE_IPSP_H)
endif()

check_include_files("sys/types.h;netipsec/ipsec.h" HAVE_NET_IPSEC_H)
if(HAVE_NET_IPSEC_H)
	add_compile_definitions(HAVE_NET_IPSEC_H)
endif()

check_include_files("sys/types.h;netinet6/ipsec.h" HAVE_NETINET6_IPSEC_H)
if(HAVE_NETINET6_IPSEC_H)
	add_compile_definitions(HAVE_NETINET6_IPSEC_H)
endif()

check_include_files("linux/ipsec.h" HAVE_LINUX_IPSEC_H)
if(HAVE_LINUX_IPSEC_H)
	add_compile_definitions(HAVE_LINUX_IPSEC_H)
endif()

check_include_files("sys/types.h;sys/queue.h;imsg.h" HAVE_IMSG_H)
if(HAVE_IMSG_H)
	add_compile_definitions(HAVE_IMSG_H)
endif()

check_symbol_exists(recallocarray "stdlib.h" HAVE_RECALLOCARRAY)
if(HAVE_RECALLOCARRAY)
	add_compile_definitions(HAVE_RECALLOCARRAY)
endif()

check_symbol_exists(reallocarray "stdlib.h" HAVE_REALLOCARRAY)
if(HAVE_REALLOCARRAY)
	add_compile_definitions(HAVE_REALLOCARRAY)
endif()

check_symbol_exists(accept4 "sys/types.h;sys/socket.h" HAVE_ACCEPT4)
if(HAVE_ACCEPT4)
	add_compile_definitions(HAVE_ACCEPT4)
endif()

check_symbol_exists(SOCK_NONBLOCK "sys/socket.h" HAVE_SOCK_NONBLOCK)
if(HAVE_SOCK_NONBLOCK)
	add_compile_definitions(HAVE_SOCK_NONBLOCK)
endif()

check_symbol_exists(setproctitle "stdlib.h" HAVE_SETPROCTITLE)
if(HAVE_SETPROCTITLE)
	add_compile_definitions(HAVE_SETPROCTITLE)
endif()

check_symbol_exists(pledge "unistd.h" HAVE_PLEDGE)
if(HAVE_PLEDGE)
	add_compile_definitions(HAVE_PLEDGE)
endif()

check_symbol_exists(setresgid "unistd.h" HAVE_SETRESGID)
if(HAVE_SETRESGID)
	add_compile_definitions(HAVE_SETRESGID)
endif()

check_symbol_exists(setresuid "unistd.h" HAVE_SETRESUID)
if(HAVE_SETRESUID)
	add_compile_definitions(HAVE_SETRESUID)
endif()

check_symbol_exists(setregid "unistd.h" HAVE_SETREGID)
if(HAVE_SETREGID)
	add_compile_definitions(HAVE_SETREGID)
endif()

check_symbol_exists(setreuid "unistd.h" HAVE_SETREUID)
if(HAVE_SETREUID)
	add_compile_definitions(HAVE_SETREUID)
endif()

check_symbol_exists(getrtable "sys/types.h;sys/socket.h" HAVE_GETRTABLE)
if(HAVE_GETRTABLE)
	add_compile_definitions(HAVE_GETRTABLE)
endif()

# setrtable
check_symbol_exists(setrtable "sys/types.h;sys/socket.h" HAVE_SETRTABLE)
if(HAVE_SETRTABLE)
	add_compile_definitions(HAVE_SETRTABLE)
endif()

if (NOT CMAKE_SYSTEM_NAME MATCHES "iOS" OR
    CMAKE_OSX_DEPLOYMENT_TARGET VERSION_GREATER_EQUAL "14.0")
# only iOS 14.0 or newer supports strtonum(3)
check_symbol_exists(strtonum "stdlib.h" HAVE_STRTONUM)
endif()
if(HAVE_STRTONUM)
	add_compile_definitions(HAVE_STRTONUM)
endif()

check_symbol_exists(ifgroupreq "net/if.h" HAVE_IFGROUPREQ)
if(HAVE_IFGROUPREQ)
	add_compile_definitions(HAVE_IFGROUPREQ)
endif()

check_symbol_exists(freezero "stdlib.h" HAVE_ACCEPT4)
if(HAVE_FREEZERO)
	add_compile_definitions(HAVE_FREEZERO)
endif()

check_symbol_exists(getdtablecount "unistd.h" HAVE_GETDTABLECOUNT)
if(HAVE_GETDTABLECOUNT)
	add_compile_definitions(HAVE_GETDTABLECOUNT)
endif()

check_symbol_exists(timespecsub "sys/time.h" HAVE_TIMESPECSUB)
if(HAVE_TIMESPECSUB)
	add_compile_definitions(HAVE_TIMESPECSUB)
endif()

check_symbol_exists(asprintf "stdio.h" HAVE_ASPRINTF)
if(HAVE_ASPRINTF)
	add_compile_definitions(HAVE_ASPRINTF)
endif()

check_symbol_exists(strcasecmp "strings.h;string.h" HAVE_STRCASECMP)
if(HAVE_STRCASECMP)
	add_compile_definitions(HAVE_STRCASECMP)
endif()

check_symbol_exists(strlcat "string.h" HAVE_STRLCAT)
if(HAVE_STRLCAT)
	add_compile_definitions(HAVE_STRLCAT)
endif()

check_symbol_exists(strlcpy "string.h" HAVE_STRLCPY)
if(HAVE_STRLCPY)
	add_compile_definitions(HAVE_STRLCPY)
endif()

check_symbol_exists(strndup "string.h" HAVE_STRNDUP)
if(HAVE_STRNDUP)
	add_compile_definitions(HAVE_STRNDUP)
endif()

check_symbol_exists(ffs "strings.h;string.h" HAVE_FFS)
if(HAVE_FFS)
	add_compile_definitions(HAVE_FFS)
endif()

check_symbol_exists(strnlen "string.h" HAVE_STRNLEN)
if(HAVE_STRNLEN)
	add_compile_definitions(HAVE_STRNLEN)
endif()

check_symbol_exists(strsep "string.h" HAVE_STRSEP)
if(HAVE_STRSEP)
	add_compile_definitions(HAVE_STRSEP)
endif()

check_symbol_exists(timegm "time.h" HAVE_TIMEGM)
if(HAVE_TIMEGM)
	add_compile_definitions(HAVE_TIMEGM)
endif()

check_symbol_exists(arc4random_buf "stdlib.h" HAVE_ARC4RANDOM_BUF)
if(HAVE_ARC4RANDOM_BUF)
	add_compile_definitions(HAVE_ARC4RANDOM_BUF)
endif()

check_symbol_exists(arc4random_uniform "stdlib.h" HAVE_ARC4RANDOM_UNIFORM)
if(HAVE_ARC4RANDOM_UNIFORM)
	add_compile_definitions(HAVE_ARC4RANDOM_UNIFORM)
endif()

check_symbol_exists(explicit_bzero "strings.h;string.h" HAVE_EXPLICIT_BZERO)
if(HAVE_EXPLICIT_BZERO)
	add_compile_definitions(HAVE_EXPLICIT_BZERO)
endif()

check_symbol_exists(getauxval "sys/auxv.h" HAVE_GETAUXVAL)
if(HAVE_GETAUXVAL)
	add_compile_definitions(HAVE_GETAUXVAL)
endif()

check_symbol_exists(getentropy "sys/random.h" HAVE_GETENTROPY)
if(HAVE_GETENTROPY)
	add_compile_definitions(HAVE_GETENTROPY)
endif()

check_symbol_exists(getpagesize "unistd.h" HAVE_GETPAGESIZE)
if(HAVE_GETPAGESIZE)
	add_compile_definitions(HAVE_GETPAGESIZE)
endif()

check_symbol_exists(getprogname "stdlib.h" HAVE_GETPROGNAME)
if(HAVE_GETPROGNAME)
	add_compile_definitions(HAVE_GETPROGNAME)
endif()

check_symbol_exists(syslog_r "syslog.h;stdarg.h" HAVE_SYSLOG_R)
if(HAVE_SYSLOG_R)
	add_compile_definitions(HAVE_SYSLOG_R)
endif()

check_symbol_exists(syslog "syslog.h" HAVE_SYSLOG)
if(HAVE_SYSLOG)
	add_compile_definitions(HAVE_SYSLOG)
endif()

check_symbol_exists(vis "vis.h" HAVE_VIS)
if(HAVE_VIS)
	add_compile_definitions(HAVE_VIS)
endif()

check_symbol_exists(timespecsub "sys/time.h" HAVE_TIMESPECSUB)
if(HAVE_TIMESPECSUB)
	add_compile_definitions(HAVE_TIMESPECSUB)
endif()

check_symbol_exists(timingsafe_bcmp "string.h" HAVE_TIMINGSAFE_BCMP)
if(HAVE_TIMINGSAFE_BCMP)
	add_compile_definitions(HAVE_TIMINGSAFE_BCMP)
endif()

check_symbol_exists(timingsafe_memcmp "string.h" HAVE_TIMINGSAFE_MEMCMP)
if(HAVE_MEMCMP)
	add_compile_definitions(HAVE_MEMCMP)
endif()

check_symbol_exists(memmem "string.h" HAVE_MEMMEM)
if(HAVE_MEMMEM)
	add_compile_definitions(HAVE_MEMMEM)
endif()

check_include_files(err.h HAVE_ERR_H)
if(HAVE_ERR_H)
	add_compile_definitions(HAVE_ERR_H)
endif()

check_symbol_exists(usleep "unistd.h" HAVE_USLEEP)
if(HAVE_USLEEP)
	add_compile_definitions(HAVE_USLEEP)
endif()

check_symbol_exists(getopt "unistd.h" HAVE_GETOPT)
if(HAVE_GETOPT)
        add_compile_definitions(HAVE_GETOPT)
endif()

check_symbol_exists(msgbuf_new_reader "imsg.h" HAVE_MSGBUF_NEW_READER)
if(HAVE_MSGBUF_NEW_READER)
        add_compile_definitions(HAVE_MSGBUF_NEW_READER)
endif()

if(HAVE_VROUTE OR HAVE_VROUTE_NETLINK)
	add_compile_definitions(HAVE_VROUTE)
endif()
if(WITH_SYSTEMD)
	add_compile_definitions(WITH_SYSTEMD)
endif()
if(WITH_APPARMOR)
	add_compile_definitions(WITH_APPARMOR)
endif()

if(ASAN)
	message("Using ASAN")
	string(APPEND CMAKE_C_FLAGS " -fno-omit-frame-pointer -fsanitize=address")
	string(APPEND CMAKE_LINKER_FLAGS " -fno-omit-frame-pointer -fsanitize=address")
endif()
if(UBSAN)
	message("Using UBSAN")
	string(APPEND CMAKE_C_FLAGS " -fno-omit-frame-pointer -fsanitize=undefined")
	string(APPEND CMAKE_LINKER_FLAGS " -fno-omit-frame-pointer -fsanitize=undefined")
endif()
if(MSAN)
	message("Using MSAN")
	string(APPEND CMAKE_C_FLAGS " -fno-omit-frame-pointer -fsanitize=memory")
	string(APPEND CMAKE_LINKER_FLAGS " -fno-omit-frame-pointer -fsanitize=memory")
endif()

if(CLUSTERFUZZ)
	message("Compiling parser-libfuzzer")
	add_subdirectory(regress/parser-libfuzzer)
	if (NOT DEFINED ENV{CLUSTERFUZZLITE})
		set(CMAKE_C_COMPILER clang)
		string(APPEND CMAKE_C_FLAGS " -g -O0 -fsanitize=fuzzer-no-link")
	endif()
endif()
