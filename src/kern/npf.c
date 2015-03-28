/*	$NetBSD: npf.c,v 1.22 2014/07/25 08:10:40 dholland Exp $	*/

/*-
 * Copyright (c) 2009-2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This material is based upon work partially supported by The
 * NetBSD Foundation under a contract with Mindaugas Rasiukevicius.
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

/*
 * NPF main: dynamic load/initialisation and unload routines.
 */

#ifdef _KERNEL
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf.c,v 1.22 2014/07/25 08:10:40 dholland Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/percpu.h>
#endif

#include "npf_impl.h"
#include "npf_conn.h"

npf_t *	npf_kernel_ctx = NULL __read_mostly;

npf_t *
npf_create(void)
{
	npf_t *npf;

	npf = kmem_zalloc(sizeof(npf_t), KM_SLEEP);
	npf->stats_percpu = percpu_alloc(NPF_STATS_SIZE);

	npf_bpf_sysinit();
	npf_worker_sysinit(npf);
	npf_tableset_sysinit();
	npf_conn_sysinit(npf);
	npf_nat_sysinit();
	npf_alg_sysinit(npf);
	npf_ext_sysinit(npf);

	/* Load an empty configuration. */
	npf_config_init(npf);
	return npf;
}

void
npf_destroy(npf_t *npf)
{
	/*
	 * Destroy the current configuration.  Note: at this point all
	 * handlers must be deactivated; we will drain any processing.
	 */
	npf_config_fini(npf);

	/* Finally, safe to destroy the subsystems. */
	npf_ext_sysfini(npf);
	npf_alg_sysfini(npf);
	npf_nat_sysfini();
	npf_conn_sysfini(npf);
	npf_tableset_sysfini();
	npf_bpf_sysfini();

	/* Note: worker is the last. */
	npf_worker_sysfini(npf);
	percpu_free(npf->stats_percpu, NPF_STATS_SIZE);
}

void
npf_setkernctx(npf_t *npf)
{
	npf_kernel_ctx = npf;
}

npf_t *
npf_getkernctx(void)
{
	return npf_kernel_ctx;
}

/*
 * NPF statistics interface.
 */

void
npf_stats_inc(npf_t *npf, npf_stats_t st)
{
	uint64_t *stats = percpu_getref(npf->stats_percpu);
	stats[st]++;
	percpu_putref(npf->stats_percpu);
}

void
npf_stats_dec(npf_t *npf, npf_stats_t st)
{
	uint64_t *stats = percpu_getref(npf->stats_percpu);
	stats[st]--;
	percpu_putref(npf->stats_percpu);
}

static void
npf_stats_collect(void *mem, void *arg, struct cpu_info *ci)
{
	uint64_t *percpu_stats = mem, *full_stats = arg;

	for (unsigned i = 0; i < NPF_STATS_COUNT; i++) {
		full_stats[i] += percpu_stats[i];
	}
}

/*
 * npf_stats: export collected statistics.
 */
int
npf_stats(npf_t *npf, void *data)
{
	uint64_t *fullst, *uptr = *(uint64_t **)data;
	int error;

	fullst = kmem_zalloc(NPF_STATS_SIZE, KM_SLEEP);
	percpu_foreach(npf->stats_percpu, npf_stats_collect, fullst);
	error = copyout(fullst, uptr, NPF_STATS_SIZE);
	kmem_free(fullst, NPF_STATS_SIZE);
	return error;
}