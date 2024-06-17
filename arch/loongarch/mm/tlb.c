// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Loongson Technology Corporation Limited
 */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/export.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/tlb.h>

void local_flush_tlb_all(void)
{
	invtlb_all_atomic(INVTLB_CURRENT_ALL, 0, 0);
}
EXPORT_SYMBOL(local_flush_tlb_all);

void local_flush_tlb_user(void)
{
	invtlb_all_atomic(INVTLB_CURRENT_GFALSE, 0, 0);
}

/* All entries common to a mm share an asid.  To effectively flush
   these entries, we just bump the asid. */
void local_flush_tlb_mm(struct mm_struct *mm)
{
	int cpu;

	preempt_disable();

	cpu = smp_processor_id();

	if (asid_valid(mm, cpu)) {
		drop_mmu_context(mm, cpu);
	} else {
		cpumask_clear_cpu(cpu, mm_cpumask(mm));
	}

	preempt_enable();
}

void local_flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
	unsigned long end)
{
	struct mm_struct *mm = vma->vm_mm;
	int cpu = smp_processor_id();

	if (asid_valid(mm, cpu)) {
		unsigned long size, flags;

		local_irq_save(flags);
		start = round_down(start, PAGE_SIZE << 1);
		end = round_up(end, PAGE_SIZE << 1);
		size = (end - start) >> (PAGE_SHIFT + 1);
		if (size <= (current_cpu_data.tlbsizestlbsets ?
			     current_cpu_data.tlbsize / 8 :
			     current_cpu_data.tlbsize / 2)) {

			int asid = cpu_asid(cpu, mm);
			while (start < end) {
				invtlb(INVTLB_ADDR_GFALSE_AND_ASID, asid, start);
				start += (PAGE_SIZE << 1);
			}
		} else {
			drop_mmu_context(mm, cpu);
		}
		local_irq_restore(flags);
	} else {
		cpumask_clear_cpu(cpu, mm_cpumask(mm));
	}
}

void local_flush_tlb_kernel_range(unsigned long start, unsigned long end)
{
	unsigned long size, flags;

	local_irq_save(flags);
	size = (end - start + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
	size = (size + 1) >> 1;
	if (size <= (current_cpu_data.tlbsizestlbsets ?
		     current_cpu_data.tlbsize / 8 :
		     current_cpu_data.tlbsize / 2)) {

		start &= (PAGE_MASK << 1);
		end += ((PAGE_SIZE << 1) - 1);
		end &= (PAGE_MASK << 1);

		while (start < end) {
			invtlb_noinfo(INVTLB_ADDR_GTRUE_OR_ASID, 0, start);
			start += (PAGE_SIZE << 1);
		}
	} else {
		invtlb_all_atomic(INVTLB_CURRENT_GTRUE, 0, 0);
	}
	local_irq_restore(flags);
}

void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long page)
{
	int cpu = smp_processor_id();

	if (asid_valid(vma->vm_mm, cpu)) {
		int newpid;

		newpid = cpu_asid(cpu, vma->vm_mm);
		page &= (PAGE_MASK << 1);
		invtlb_atomic(INVTLB_ADDR_GFALSE_AND_ASID, newpid, page);
	} else {
		cpumask_clear_cpu(cpu, mm_cpumask(vma->vm_mm));
	}
}

/*
 * This one is only used for pages with the global bit set so we don't care
 * much about the ASID.
 */
void local_flush_tlb_one(unsigned long page)
{
	page &= (PAGE_MASK << 1);
	invtlb_noinfo_atomic(INVTLB_ADDR_GTRUE_OR_ASID, 0, page);
}

static void __update_hugetlb(struct vm_area_struct *vma, unsigned long address, pte_t *ptep)
{
	unsigned long flags;
	int idx;
	unsigned long lo;

	local_irq_save(flags);

	address &= (PAGE_MASK << 1);
	write_csr_entryhi(address);
	tlb_probe();
	idx = read_csr_tlbidx();

	write_csr_pagesize(PS_HUGE_SIZE);
	lo = pmd_to_entrylo(pte_val(*ptep));
	write_csr_entrylo0(lo);
	write_csr_entrylo1(lo + (HPAGE_SIZE >> 1));

	if (idx < 0)
		tlb_write_random();
	else
		tlb_write_indexed();
	write_csr_pagesize(PS_DEFAULT_SIZE);
	local_irq_restore(flags);
}

void __update_tlb(struct vm_area_struct *vma, unsigned long address, pte_t *ptep)
{
	unsigned long flags;
	int idx;

	if (cpu_has_ptw)
		return;
	/*
	 * Handle debugger faulting in for debugee.
	 */
	if (current->active_mm != vma->vm_mm)
		return;

	if (pte_val(*ptep) & _PAGE_HUGE)
		return __update_hugetlb(vma, address, ptep);

	local_irq_save(flags);
	if ((unsigned long)ptep & sizeof(pte_t))
		ptep--;

	address &= (PAGE_MASK << 1);
	write_csr_entryhi(address);
	tlb_probe();
	idx = read_csr_tlbidx();
	write_csr_pagesize(PS_DEFAULT_SIZE);
	write_csr_entrylo0(pte_to_entrylo(pte_val(*ptep++)));
	write_csr_entrylo1(pte_to_entrylo(pte_val(*ptep)));
	if (idx < 0)
		tlb_write_random();
	else
		tlb_write_indexed();
	local_irq_restore(flags);
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE

int has_transparent_hugepage(void)
{
	static unsigned int size = -1;

	if (size == -1) {	/* first call comes during __init */
		unsigned long flags;

		local_irq_save(flags);
		write_csr_pagesize(PS_HUGE_SIZE);
		size = read_csr_pagesize();
		write_csr_pagesize(PS_DEFAULT_SIZE);
		local_irq_restore(flags);
	}
	return size == PS_HUGE_SIZE;
}

#endif /* CONFIG_TRANSPARENT_HUGEPAGE  */

static void setup_pw(void)
{
	unsigned long pwctl0, pwctl1;
	unsigned long pgd_i = 0, pgd_w = 0;
	unsigned long pud_i = 0, pud_w = 0;
	unsigned long pmd_i = 0, pmd_w = 0;
	unsigned long pte_i = 0, pte_w = 0;

	pgd_i = PGDIR_SHIFT;
	pgd_w = PAGE_SHIFT - 3;
#if CONFIG_PGTABLE_LEVELS > 3
	pud_i = PUD_SHIFT;
	pud_w = PAGE_SHIFT - 3;
#endif
#if CONFIG_PGTABLE_LEVELS > 2
	pmd_i = PMD_SHIFT;
	pmd_w = PAGE_SHIFT - 3;
#endif
	pte_i = PAGE_SHIFT;
	pte_w = PAGE_SHIFT - 3;

	pwctl0 = pte_i | pte_w << 5 | pmd_i << 10 | pmd_w << 15 | pud_i << 20 | pud_w << 25;
	pwctl1 = pgd_i | pgd_w << 6;

	csr_write64(pwctl0, LOONGARCH_CSR_PWCTL0);
	if (cpu_has_ptw)
		csr_write64(pwctl1 | CSR_PWCTL1_PTW, LOONGARCH_CSR_PWCTL1);
	else
		csr_write64(pwctl1, LOONGARCH_CSR_PWCTL1);
}

/*
 * pgtable bits are assigned dynamically depending on processor feature
 * and statically based on kernel configuration.  This spits out the actual
 * values the kernel is using.	Required to make sense from disassembled
 * TLB exception handlers.
 */
static void output_pgtable_bits_defines(void)
{
#define pr_define(fmt, ...)					\
	pr_debug("#define " fmt, ##__VA_ARGS__)

	pr_debug("#include <asm/asm.h>\n");
	pr_debug("#include <asm/regdef.h>\n");
	pr_debug("\n");

	pr_define("_PAGE_VALID_SHIFT %d\n", _PAGE_VALID_SHIFT);
	pr_define("_PAGE_DIRTY_SHIFT %d\n", _PAGE_DIRTY_SHIFT);
	pr_define("_PAGE_HUGE_SHIFT %d\n", _PAGE_HUGE_SHIFT);
	pr_define("_PAGE_GLOBAL_SHIFT %d\n", _PAGE_GLOBAL_SHIFT);
	pr_define("_PAGE_PRESENT_SHIFT %d\n", _PAGE_PRESENT_SHIFT);
	pr_define("_PAGE_WRITE_SHIFT %d\n", _PAGE_WRITE_SHIFT);
	pr_define("_PAGE_NO_READ_SHIFT %d\n", _PAGE_NO_READ_SHIFT);
	pr_define("_PAGE_NO_EXEC_SHIFT %d\n", _PAGE_NO_EXEC_SHIFT);
	pr_define("_PFN_SHIFT %d\n", _PFN_SHIFT);
	pr_debug("\n");
}

#ifdef CONFIG_NUMA
/* optimize tlb refill code with per-cpu entry */
static unsigned long tlbrefill_opt[NR_CPUS];

static inline void tlbrefill_reinit(unsigned int cpu)
{
	if (tlbrefill_opt[cpu] == 0) {
		struct page *page;
		void *page_addr;

		page = alloc_pages_node(cpu_to_node(cpu), GFP_KERNEL, 0);
		if (!page) {
			pr_err("Can't init per-cpu tlbrefill eentry for cpu:%d\n", cpu);
			tlbrefill_opt[cpu] = tlbrentry;
		} else {
			page_addr = page_address(page);

			memcpy((void *)page_addr, (void *)tlbrentry, 0x80);
			tlbrefill_opt[cpu] = virt_to_phys(page_addr);

			local_flush_icache_range((unsigned long)page_addr,
					(unsigned long)page_addr + 0x80);
		}
	}

	csr_write64(tlbrefill_opt[cpu], LOONGARCH_CSR_TLBRENTRY);
}
#else
#define tlbrefill_reinit(cpu) do {} while (0)
#endif

static void build_tlb_handler(int cpu)
{
	setup_pw();
	local_flush_tlb_all();
	csr_write64((long)swapper_pg_dir, LOONGARCH_CSR_PGDH);
	csr_write64((long)invalid_pgd, LOONGARCH_CSR_PGDL);
	csr_write64((long)smp_processor_id(), LOONGARCH_CSR_TMID);

	if (cpu == 0) {
		output_pgtable_bits_defines();

		set_tlb_handler();

		/* initial swapper_pg_dir so that refill can work */
		pagetable_init();
	} else {
		tlbrefill_reinit(cpu);
	}
}

void tlb_init(int cpu)
{
	write_csr_pagesize(PS_DEFAULT_SIZE);
	write_csr_stlbpgsize(PS_DEFAULT_SIZE);
	write_csr_tlbrefill_pagesize(PS_DEFAULT_SIZE);

	if (read_csr_pagesize() != PS_DEFAULT_SIZE)
		panic("MMU doesn't support PAGE_SIZE=0x%lx", PAGE_SIZE);

	/*
	 * assign initial value for asid_cache(cpu) on booting stage
	 * For hotplugged cpu, keep unchanged since cpu_context(cpu, mm)
	 * for all threads is not clear.
	 * For better performance, it should be initial value of next round
	 * since local tlb is flushed, asid of threads scheduled on hotplugged
	 * cpu will be recalculated, like this:
	 *    (asid_cache(cpu) + asid_mask + 1) & ~asid_mask
	 */
	if (!asid_cache(cpu))
		asid_cache(cpu) = asid_first_version(cpu);
	cpu_context(cpu, &init_mm) = asid_cache(cpu);
	write_csr_asid(cpu_asid(cpu, &init_mm));
	csr_write64((unsigned long)invalid_pgd, LOONGARCH_CSR_PGDL);

	build_tlb_handler(cpu);
}
