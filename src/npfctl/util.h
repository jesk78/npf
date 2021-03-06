/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_UTIL_H_
#define	_UTIL_H_

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>

#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>

#include "../kern/stand/cext.h"

#ifndef __CTASSERT
#define	__CTASSERT		CTASSERT
#endif

#ifndef ecalloc
static inline void *
ecalloc(size_t n, size_t s)
{
	void *p = calloc(n, s);
	if (p == NULL && n != 0 && s != 0)
		err(1, "Cannot allocate %zu blocks of size %zu", n, s);
	return p;
}
#endif

#ifndef erealloc
static inline void *
erealloc(void *p, size_t n)
{
	void *q = realloc(p, n);
	if (q == NULL && n != 0)
		err(1, "Cannot re-allocate %zu bytes", n);
	return q;
}
#endif

#ifndef estrdup
static inline char *
estrdup(const char *s)
{
	char *d = strdup(s);
	if (d == NULL)
		err(1, "Cannot copy string");
	return d;
}
#endif

#ifndef estrndup
static inline char *
estrndup(const char *s, size_t len)
{
	char *d = strndup(s, len);
	if (d == NULL)
		err(1, "Cannot copy string");
	return d;
}
#endif

#if defined(__linux__) && !defined(_GNU_SOURCE) // XXX glibc
int asprintf(char **strp, const char *fmt, ...);
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif

#ifndef easprintf
static inline int
easprintf(char ** __restrict ret, const char * __restrict format, ...)
{
	int rv;
	va_list ap;
	va_start(ap, format);
	if ((rv = vasprintf(ret, format, ap)) == -1)
		err(1, "Cannot format string");
	va_end(ap);
	return rv;
}
#endif

#ifdef __linux__

static inline char *
strlcpy(char *d, const char *s, size_t l)
{
	if (l == 0)
		return NULL;
	strncpy(d, s, l);
	d[l - 1] = '\0';
	return d;
}

# define SALEN(sa)	((unsigned)sizeof(*sa))

static int
debug_in(char *str, size_t len, const struct sockaddr_in *sin)
{
	return snprintf(str, len, "sin_len=%u, sin_family=%u, sin_port=%u, "
	    "sin_addr.s_addr=%08x",
	    SALEN(sin), sin->sin_family, sin->sin_port,
	    sin->sin_addr.s_addr);
}

static int
debug_in6(char *str, size_t len, const struct sockaddr_in6 *sin6)
{
	const uint8_t *s = sin6->sin6_addr.s6_addr;

	return snprintf(str, len, "sin6_len=%u, sin6_family=%u, sin6_port=%u, "
	    "sin6_flowinfo=%u, "
	    "sin6_addr=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:"
	    "%02x:%02x:%02x:%02x:%02x:%02x, sin6_scope_id=%u",
	    SALEN(sin6), sin6->sin6_family, sin6->sin6_port,
	    sin6->sin6_flowinfo, s[0x0], s[0x1], s[0x2], s[0x3], s[0x4], s[0x5],
	    s[0x6], s[0x7], s[0x8], s[0x9], s[0xa], s[0xb], s[0xc], s[0xd],
	    s[0xe], s[0xf], sin6->sin6_scope_id);
}

static int
debug_un(char *str, size_t len, const struct sockaddr_un *sun)
{
	return snprintf(str, len, "sun_len=%u, sun_family=%u, sun_path=%*s",
	    SALEN(sun), sun->sun_family, (int)sizeof(sun->sun_path),
	    sun->sun_path);
}

static inline int
sockaddr_snprintf(char * const sbuf, const size_t len, const char * const fmt,
    const struct sockaddr * const sa)
{
	const void *a = NULL;
	char abuf[1024], nbuf[1024], *addr = NULL;
	char Abuf[1024], pbuf[32], *name = NULL, *port = NULL;
	char *ebuf = &sbuf[len - 1], *buf = sbuf;
	const char *ptr, *s;
	size_t salen;
	int p = -1;
#ifdef HAVE_NETATALK_AT_H
	const struct sockaddr_at *sat = NULL;
#endif
	const struct sockaddr_in *sin4 = NULL;
	const struct sockaddr_in6 *sin6 = NULL;
	const struct sockaddr_un *sun = NULL;
#ifdef HAVE_NET_IF_DL_H
	const struct sockaddr_dl *sdl = NULL;
	char *w = NULL;
#endif
	int na = 1;

#define ADDC(c) do { if (buf < ebuf) *buf++ = c; else buf++; } \
	while (/*CONSTCOND*/0)
#define ADDS(p) do { for (s = p; *s; s++) ADDC(*s); } \
	while (/*CONSTCOND*/0)
#define ADDNA() do { if (na) ADDS("N/A"); } \
	while (/*CONSTCOND*/0)

	switch (sa->sa_family) {
	case AF_UNSPEC:
		goto done;
#ifdef HAVE_NETATALK_AT_H
	case AF_APPLETALK:
		salen = sizeof(*sat);
		sat = ((const struct sockaddr_at *)(const void *)sa);
		p = ntohs(sat->sat_port);
		(void)snprintf(addr = abuf, sizeof(abuf), "%u.%u",
			ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node);
		(void)snprintf(port = pbuf, sizeof(pbuf), "%d", p);
		break;
#endif
	case AF_LOCAL:
		salen = sizeof(*sun);
		sun = ((const struct sockaddr_un *)(const void *)sa);
		(void)strlcpy(addr = abuf, sun->sun_path, sizeof(abuf));
		break;
	case AF_INET:
		salen = sizeof(*sin4);
		sin4 = ((const struct sockaddr_in *)(const void *)sa);
		p = ntohs(sin4->sin_port);
		a = &sin4->sin_addr;
		break;
	case AF_INET6:
		salen = sizeof(*sin6);
		sin6 = ((const struct sockaddr_in6 *)(const void *)sa);
		p = ntohs(sin6->sin6_port);
		a = &sin6->sin6_addr;
		break;
#ifdef HAVE_NET_IF_DL_H
	case AF_LINK:
		sdl = ((const struct sockaddr_dl *)(const void *)sa);
		addr = abuf;
		if (sdl->sdl_slen == 0 && sdl->sdl_nlen == 0
		    && sdl->sdl_alen == 0) {
			salen = sizeof(*sdl);
			(void)snprintf(abuf, sizeof(abuf), "link#%hu",
			    sdl->sdl_index);
		} else {
			salen = sdl->sdl_slen + sdl->sdl_nlen +  sdl->sdl_alen;
			if (salen < sizeof(*sdl))
				salen = sizeof(*sdl);
			(void)strlcpy(abuf, link_ntoa(sdl), sizeof(abuf));
			if ((w = strchr(addr, ':')) != NULL) {
			    *w++ = '\0';
			    addr = w;
			}
		}
		break;
#endif
	default:
#ifdef __linux__
		errno = ENOTSUP;
#else
		errno = EAFNOSUPPORT;
#endif
		return -1;
	}

	if (addr == abuf)
		name = addr;

	if (a && getnameinfo(sa, (socklen_t)salen, addr = abuf,
	    (unsigned int)sizeof(abuf), NULL, 0,
	    NI_NUMERICHOST|NI_NUMERICSERV) != 0)
		return -1;

	for (ptr = fmt; *ptr; ptr++) {
		if (*ptr != '%') {
			ADDC(*ptr);
			continue;
		}
	  next_char:
		switch (*++ptr) {
		case '?':
			na = 0;
			goto next_char;
		case 'a':
			ADDS(addr);
			break;
		case 'p':
			if (p != -1) {
				(void)snprintf(nbuf, sizeof(nbuf), "%d", p);
				ADDS(nbuf);
			} else
				ADDNA();
			break;
		case 'f':
			(void)snprintf(nbuf, sizeof(nbuf), "%d", sa->sa_family);
			ADDS(nbuf);
			break;
		case 'l':
			(void)snprintf(nbuf, sizeof(nbuf), "%zu", salen);
			ADDS(nbuf);
			break;
		case 'A':
			if (name)
				ADDS(name);
			else if (!a)
				ADDNA();
			else {
				getnameinfo(sa, (socklen_t)salen, name = Abuf,
					(unsigned int)sizeof(nbuf), NULL, 0, 0);
				ADDS(name);
			}
			break;
		case 'P':
			if (port)
				ADDS(port);
			else if (p == -1)
				ADDNA();
			else {
				getnameinfo(sa, (socklen_t)salen, NULL, 0,
					port = pbuf,
					(unsigned int)sizeof(pbuf), 0);
				ADDS(port);
			}
			break;
		case 'I':
#ifdef HAVE_NET_IF_DL_H
			if (sdl && addr != abuf) {
				ADDS(abuf);
			} else
#endif
			{
				ADDNA();
			}
			break;
		case 'F':
			if (sin6) {
				(void)snprintf(nbuf, sizeof(nbuf), "%d",
				    sin6->sin6_flowinfo);
				ADDS(nbuf);
				break;
			} else {
				ADDNA();
			}
			break;
		case 'S':
			if (sin6) {
				(void)snprintf(nbuf, sizeof(nbuf), "%d",
				    sin6->sin6_scope_id);
				ADDS(nbuf);
				break;
			} else {
				ADDNA();
			}
			break;
		case 'R':
#ifdef HAVE_NETATALK_AT_H
			if (sat) {
				const struct netrange *n =
				    &sat->sat_range.r_netrange;
				(void)snprintf(nbuf, sizeof(nbuf),
				    "%d:[%d,%d]", n->nr_phase , n->nr_firstnet,
				    n->nr_lastnet);
				ADDS(nbuf);
			} else
#endif
			{
				ADDNA();
			}
			break;
		case 'D':
			switch (sa->sa_family) {
#ifdef HAVE_NETATALK_AT_H
			case AF_APPLETALK:
				debug_at(nbuf, sizeof(nbuf), sat);
				break;
#endif
			case AF_LOCAL:
				debug_un(nbuf, sizeof(nbuf), sun);
				break;
			case AF_INET:
				debug_in(nbuf, sizeof(nbuf), sin4);
				break;
			case AF_INET6:
				debug_in6(nbuf, sizeof(nbuf), sin6);
				break;
#ifdef HAVE_NET_IF_DL_H
			case AF_LINK:
				debug_dl(nbuf, sizeof(nbuf), sdl);
				break;
#endif
			default:
				abort();
			}
			ADDS(nbuf);
			break;
		default:
			ADDC('%');
			if (na == 0)
				ADDC('?');
			if (*ptr == '\0')
				goto done;
			/*FALLTHROUGH*/
		case '%':
			ADDC(*ptr);
			break;
		}
		na = 1;
	}
done:
	if (buf < ebuf)
		*buf = '\0';
	else if (len != 0)
		sbuf[len - 1] = '\0';
	return (int)(buf - sbuf);
}
#endif

#endif
