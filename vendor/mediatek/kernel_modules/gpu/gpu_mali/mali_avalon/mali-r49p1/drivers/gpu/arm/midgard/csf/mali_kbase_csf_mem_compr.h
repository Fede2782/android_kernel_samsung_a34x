/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 *
 * (C) COPYRIGHT 2024 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mali
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mali_kbase_csf_mem_compr
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH csf

#if !defined(_KBASE_CSF_MEM_COMPR_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _KBASE_CSF_MEM_COMPR_H_

#include <linux/tracepoint.h>

struct kbase_context;
struct kbase_va_region;
struct tagged_addr;

/**
 * graphics_mem_swapout - Emitted after process compression done.
 * Event that occurs when the per-process graphics memory swap-out procedure finishes
 *
 * @pid: Unix process ID
 * @nr_limit_swapout: the limit of the number of reclaimable pages. (unit: pages)
 * @errno: error number if any error occurs during this procedure; 0 otherwise.
 * @nr_reclaimed:the number of reclaimed pages during the procedure. (unit: pages)
 * @latency_us:the total latency to execute the procedure (unit: microseconds)
 */
TRACE_EVENT(graphics_mem_swapout,
	    TP_PROTO(pid_t pid, size_t nr_limit_swapout, int errno, size_t nr_reclaimed,
		     int latency_us),
	    TP_ARGS(pid, nr_limit_swapout, errno, nr_reclaimed, latency_us),
	    TP_STRUCT__entry(__field(pid_t, pid) __field(size_t, nr_limit_swapout)
				     __field(int, errno) __field(size_t, nr_reclaimed)
					     __field(int, latency_us)),
	    TP_fast_assign(__entry->pid = pid; __entry->nr_limit_swapout = nr_limit_swapout;
			   __entry->errno = errno; __entry->nr_reclaimed = nr_reclaimed;
			   __entry->latency_us = latency_us;),
	    TP_printk("pid=%d nr_limit_swapout=%ld errno=%d nr_reclaimed=%ld latency_us=%d",
		      __entry->pid, __entry->nr_limit_swapout, __entry->errno,
		      __entry->nr_reclaimed, __entry->latency_us));
/**
 * graphics_mem_swapin - Emitted after process decompression done.
 * Event that occurs when the per-process graphics memory swap-in procedure finishes
 *
 * @pid: Unix process ID
 * @nr_swapin:the number of swapped-in pages during the procedure. (unit: pages)
 * @latency_us:the total latency to execute the procedure (unit: microseconds)
 * @skip_account:
 */
TRACE_EVENT(graphics_mem_swapin, TP_PROTO(pid_t pid, size_t nr_swapin, int latency_us, bool skip_account),
	    TP_ARGS(pid, nr_swapin, latency_us, skip_account),
	    TP_STRUCT__entry(__field(pid_t, pid) __field(size_t, nr_swapin)
				     __field(int, latency_us) __field(bool, skip_account)),
	    TP_fast_assign(__entry->pid = pid; __entry->nr_swapin = nr_swapin;
			   __entry->latency_us = latency_us; __entry->skip_account = skip_account;),
	    TP_printk("pid=%d nr_swapin=%ld latency_us=%d skip_account=%d", __entry->pid, __entry->nr_swapin,
		      __entry->latency_us, __entry->skip_account));

#if IS_ENABLED(CONFIG_MALI_MEMORY_COMPRESSION)
char *kbase_zs_backend_get(struct device *dev);

int kbase_zs_compress(struct kbase_context *kctx);
int kbase_zs_decompress(struct kbase_context *kctx);
int kbase_zs_decompress_region(struct kbase_context *kctx, struct kbase_va_region *reg,
			       bool skip_flush, bool skip_account);
void kbase_zs_free_compressed_page(struct kbase_context *kctx, struct tagged_addr *pages);

/**
 * kbase_csf_mem_compr_init() - Initialize kbase CSF memory compression
 * @kctx:   Pointer to the kbase context.
 *
 * This function must be called only when a kbase context is instantiated.
 *
 * Return: 0 on success.
 */
int kbase_csf_mem_compr_init(struct kbase_context *kctx);

/**
 * kbase_csf_mem_compr_term() - Terminate kbase CSF memory compression
 * @kctx:   Pointer to the kbase context.
 *
 * This function must be called only when a kbase context is terminated.
 */
void kbase_csf_mem_compr_term(struct kbase_context *kctx);

int kbase_csf_mem_compr_sysfs_init(struct kbase_context *kctx);
void kbase_csf_mem_compr_sysfs_term(struct kbase_context *kctx);
void kbase_csf_mem_compr_kobj_init(struct kbase_context *kctx);
#endif

#endif /* _KBASE_CSF_MEM_COMPR_H_ */

#include <trace/define_trace.h>