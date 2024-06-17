/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Loongson Technology Corporation Limited
 */
#ifndef _ASM_CACHEFLUSH_H
#define _ASM_CACHEFLUSH_H

#include <linux/mm.h>
#include <asm/cpu-features.h>
#include <asm/cacheops.h>

#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 0

void local_flush_icache_range(unsigned long start, unsigned long end);
void flush_cache_line_hit(unsigned long addr);
asmlinkage void cpu_flush_caches(void);

#define invalid_cache_line_hit(addr)	flush_cache_line_hit(addr)

#define flush_icache_range	local_flush_icache_range
#define flush_icache_user_range	local_flush_icache_range

extern void copy_to_user_page(struct vm_area_struct *vma,
	struct page *page, unsigned long vaddr, void *dst, const void *src,
	unsigned long len);

extern void copy_from_user_page(struct vm_area_struct *vma,
	struct page *page, unsigned long vaddr, void *dst, const void *src,
	unsigned long len);


#define flush_cache_all()				do { } while (0)
#define flush_cache_mm(mm)				do { } while (0)
#define flush_cache_dup_mm(mm)				do { } while (0)
#define flush_cache_range(vma, start, end)		do { } while (0)
#define flush_cache_page(vma, vmaddr, pfn)		do { } while (0)
#define flush_cache_vmap(start, end)			do { } while (0)
#define flush_cache_vunmap(start, end)			do { } while (0)
#define flush_icache_page(vma, page)			do { } while (0)
#define flush_icache_user_page(vma, page, addr, len)	do { } while (0)
#define flush_dcache_page(page)				do { } while (0)
#define flush_dcache_mmap_lock(mapping)			do { } while (0)
#define flush_dcache_mmap_unlock(mapping)		do { } while (0)

#define cache_op(op,addr)						\
	__asm__ __volatile__(						\
	"	cacop	%0, %1					\n"	\
	:								\
	: "i" (op), "ZC" (*(unsigned char *)(addr)))

static inline bool cache_present(struct cache_desc *cdesc)
{
	return cdesc->flags & CACHE_PRESENT;
}

static inline bool cache_private(struct cache_desc *cdesc)
{
	return cdesc->flags & CACHE_PRIVATE;
}

static inline bool cache_inclusive(struct cache_desc *cdesc)
{
	return cdesc->flags & CACHE_INCLUSIVE;
}

static inline unsigned int cpu_last_level_cache_line_size(void)
{
	unsigned int cache_present = current_cpu_data.cache_leaves_present;

	return current_cpu_data.cache_leaves[cache_present - 1].linesz;
}
#endif /* _ASM_CACHEFLUSH_H */
