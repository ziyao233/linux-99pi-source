/*
 * Copyright (C) 2010 Loongson Inc. & Lemote Inc. &
 *                    Institute of Computing Technology
 * Author:  Xiang Gao, gaoxiang@ict.ac.cn
 *          Huacai Chen, chenhc@lemote.com
 *          Xiaofu Meng, Shuangshuang Zhang
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/export.h>
#include <linux/nodemask.h>
#include <linux/swap.h>
#include <linux/memblock.h>
#include <linux/bootmem.h>
#include <linux/pfn.h>
#include <linux/highmem.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/sections.h>
#include <linux/irq.h>
#include <asm/bootinfo.h>
#include <asm/mc146818-time.h>
#include <asm/time.h>
#include <asm/wbflush.h>
#include <boot_param.h>
#include <loongson.h>
#include <asm/numa.h>
#include <linux/acpi.h>
static struct node_data prealloc__node_data[MAX_NUMNODES];
unsigned char __node_distances[MAX_NUMNODES][MAX_NUMNODES];
EXPORT_SYMBOL(__node_distances);
struct node_data *__node_data[MAX_NUMNODES];
EXPORT_SYMBOL(__node_data);
int numa_off;
extern struct loongsonlist_mem_map *loongson_mem_map;
extern unsigned int has_systab;
extern unsigned long systab_addr;

static int phy_index = 0;
static int dma_index = 0;
static struct numa_meminfo numa_meminfo;
static cpumask_t cpus_on_node[MAX_NUMNODES];
struct dma_mem_map ls_phy_map[LOONGSON3_BOOT_MEM_MAP], ls_dma_map[LOONGSON3_BOOT_MEM_MAP];
/*
 * apicid, cpu, node mappings
 */
s16 __cpuid_to_node[CONFIG_NR_CPUS] = {
	[0 ... CONFIG_NR_CPUS - 1] = NUMA_NO_NODE
};

nodemask_t numa_nodes_parsed __initdata;

void __init numa_add_cpu(int cpuid, s16 node)
{
	cpumask_set_cpu(cpuid, &cpus_on_node[node]);
}

static int __init numa_add_memblk_to(int nid, u64 start, u64 end,
				     struct numa_meminfo *mi)
{
	/* ignore zero length blks */
	if (start == end)
		return 0;

	/* whine about and ignore invalid blks */
	if (start > end || nid < 0 || nid >= MAX_NUMNODES) {
		pr_warning("NUMA: Warning: invalid memblk node %d [mem %#010Lx-%#010Lx]\n",
			   nid, start, end - 1);
		return 0;
	}

	if (mi->nr_blks >= NR_NODE_MEMBLKS) {
		pr_err("NUMA: too many memblk ranges\n");
		return -EINVAL;
	}

	mi->blk[mi->nr_blks].start = PFN_ALIGN(start);
	mi->blk[mi->nr_blks].end = PFN_ALIGN(end - PAGE_SIZE + 1);
	mi->blk[mi->nr_blks].nid = nid;
	mi->nr_blks++;
	return 0;
}

/**
 * numa_add_memblk - Add one numa_memblk to numa_meminfo
 * @nid: NUMA node ID of the new memblk
 * @start: Start address of the new memblk
 * @end: End address of the new memblk
 *
 * Add a new memblk to the default numa_meminfo.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
int __init numa_add_memblk(int nid, u64 start, u64 end)
{
	return numa_add_memblk_to(nid, start, end, &numa_meminfo);
}
static void enable_lpa(void)
{
	unsigned long value;

	value = __read_32bit_c0_register($16, 3);
	value |= 0x00000080;
	__write_32bit_c0_register($16, 3, value);
	value = __read_32bit_c0_register($16, 3);
	pr_info("CP0_Config3: CP0 16.3 (0x%lx)\n", value);

	value = __read_32bit_c0_register($5, 1);
	value |= 0x20000000;
	__write_32bit_c0_register($5, 1, value);
	value = __read_32bit_c0_register($5, 1);
	pr_info("CP0_PageGrain: CP0 5.1 (0x%lx)\n", value);
}

static void cpu_node_probe(void)
{
	int i;

	nodes_clear(node_possible_map);
	nodes_clear(node_online_map);
	for (i = 0; i < loongson_sysconf.nr_nodes; i++) {
		node_set_state(num_online_nodes(), N_POSSIBLE);
		node_set_online(num_online_nodes());
	}

	pr_info("NUMA: Discovered %d cpus on %d nodes\n",
		loongson_sysconf.nr_cpus, num_online_nodes());
}

static int __init compute_node_distance(int row, int col)
{
	int package_row = row * loongson_sysconf.cores_per_node /
				loongson_sysconf.cores_per_package;
	int package_col = col * loongson_sysconf.cores_per_node /
				loongson_sysconf.cores_per_package;

	if (col == row)
		return 0;
	else if (package_row == package_col)
		return 40;
	else
		return 200;
}

static void __init init_topology_matrix(void)
{
	int row, col;

	for (row = 0; row < MAX_NUMNODES; row++)
		for (col = 0; col < MAX_NUMNODES; col++)
			__node_distances[row][col] = -1;

	for_each_online_node(row) {
		for_each_online_node(col) {
			__node_distances[row][col] =
				compute_node_distance(row, col);
		}
	}
}

static unsigned long nid_to_addroffset(unsigned int nid)
{
	unsigned long result;
	switch (nid) {
	case 0:
	default:
		result = NODE0_ADDRSPACE_OFFSET;
		break;
	case 1:
		result = NODE1_ADDRSPACE_OFFSET;
		break;
	case 2:
		result = NODE2_ADDRSPACE_OFFSET;
		break;
	case 3:
		result = NODE3_ADDRSPACE_OFFSET;
		break;
	}
	return result;
}

static void __init add_memory_region_dma(struct dma_mem_map *loongson_map, u64 start, u64 size, long type, int *index)
{
	loongson_map[*index].mem_start = start;
	loongson_map[*index].mem_size = size;
	loongson_map[*index].mem_type = type;
	*index += 1;
}

static void __init reserve_video_ram(unsigned long mem_start, unsigned long mem_size)
{
	int node;
	unsigned long start_pfn, end_pfn, ts, te;

	if (unlikely(mem_size == 0))
		return;

	ts = PFN_DOWN(mem_start);
	te = PFN_UP(mem_start + mem_size);

	for_each_node_mask(node, node_possible_map) {

		get_pfn_range_for_nid(node, &start_pfn, &end_pfn);

		/*
 		* case0: Don't add VRAM into reserved memblock when it's OUT OF the memory region,
 		* so that bootmem can't fetch VRAM from reserved memblock to reserve it.(Reserve a region
 		* beyond memory will lead to BUG())
 		*
 		* case1: Add VRAM into reserved memblock when it's INSIDE the memory region, so that bootmem
 		* will reserve the VRAM and don't use it.
 		*
 		* case2: Add VRAM into reserved memblock when it ACROSSES the memory region, so that the
 		* error case can be checked out.(The kernel will BUG() when bootmem reserve the VRAM 
 		* memblock because of part of it beyond memory.)
 		*
 		* */
		if ((te >= start_pfn) && (ts < end_pfn)) {
			add_memory_region(mem_start, mem_size, BOOT_MEM_RESERVED);
			memblock_reserve(mem_start, mem_size);
		}
	}
}

static void __init szmem(unsigned int node)
{
	u32 i, mem_type;
	static unsigned long num_physpages = 0;
	u64 node_id, node_psize, start_pfn, end_pfn, mem_start, mem_size;

	/* Parse memory information and activate */
	for (i = 0; i < loongson_memmap->nr_map; i++) {
		node_id = loongson_memmap->map[i].node_id;
		if (node_id != node)
			continue;

		mem_type = loongson_memmap->map[i].mem_type;
		mem_size = loongson_memmap->map[i].mem_size;
		mem_start = loongson_memmap->map[i].mem_start;

		switch (mem_type) {
		case SYSTEM_RAM_LOW:
			if (node_id == 0)
				loongson_sysconf.low_physmem_start =
					loongson_memmap->map[i].mem_start;
			start_pfn = ((node_id << 44) + mem_start) >> PAGE_SHIFT;
			node_psize = (mem_size << 20) >> PAGE_SHIFT;
			end_pfn  = start_pfn + node_psize;
			num_physpages += node_psize;
			pr_info("Node%d: mem_type:%d, mem_start:0x%llx, mem_size:0x%llx MB\n",
				(u32)node_id, mem_type, mem_start, mem_size);
			pr_info("       start_pfn:0x%llx, end_pfn:0x%llx, num_physpages:0x%lx\n",
				start_pfn, end_pfn, num_physpages);
			add_memory_region((node_id << 44) + mem_start,
				(u64)mem_size << 20, BOOT_MEM_RAM);
			add_memory_region_dma(ls_phy_map, nid_to_addroffset(node_id) + loongson_memmap->map[i].mem_start,
				(u64)loongson_memmap->map[i].mem_size << 20, SYSTEM_RAM_LOW, &phy_index);
			memblock_add_node(PFN_PHYS(start_pfn),
				PFN_PHYS(end_pfn - start_pfn), node);
			break;
		case SYSTEM_RAM_HIGH:
			if (node_id == 0)
				loongson_sysconf.high_physmem_start =
					loongson_memmap->map[i].mem_start;
			start_pfn = ((node_id << 44) + mem_start) >> PAGE_SHIFT;
			node_psize = (mem_size << 20) >> PAGE_SHIFT;
			end_pfn  = start_pfn + node_psize;
			num_physpages += node_psize;
			pr_info("Node%d: mem_type:%d, mem_start:0x%llx, mem_size:0x%llx MB\n",
				(u32)node_id, mem_type, mem_start, mem_size);
			pr_info("       start_pfn:0x%llx, end_pfn:0x%llx, num_physpages:0x%lx\n",
				start_pfn, end_pfn, num_physpages);
			add_memory_region((node_id << 44) + mem_start,
				(u64)mem_size << 20, BOOT_MEM_RAM);
			add_memory_region_dma(ls_phy_map, nid_to_addroffset(node_id) + loongson_memmap->map[i].mem_start,
				(u64)loongson_memmap->map[i].mem_size << 20, SYSTEM_RAM_HIGH, &phy_index);
			memblock_add_node(PFN_PHYS(start_pfn),
				PFN_PHYS(end_pfn - start_pfn), node);
			break;
		case SYSTEM_RAM_RESERVED:
			pr_info("Node%d: mem_type:%d, mem_start:0x%llx, mem_size:0x%llx MB\n",
				(u32)node_id, mem_type, mem_start, mem_size);
			add_memory_region((node_id << 44) + mem_start,
				(u64)mem_size << 20, BOOT_MEM_RESERVED);
			memblock_reserve(((node_id << 44) + mem_start),
				mem_size << 20);
			break;

		case SMBIOS_TABLE:
			has_systab = 1;
			systab_addr = mem_start;
			add_memory_region((node_id << 44) + mem_start,
				0x2000, BOOT_MEM_RESERVED);
			memblock_reserve(((node_id << 44) + mem_start), 0x2000);
			break;

		case UMA_VIDEO_RAM:
			loongson_sysconf.vram_type = VRAM_TYPE_UMA;
			loongson_sysconf.uma_vram_addr = mem_start;
			loongson_sysconf.uma_vram_size = mem_size << 20;
			break;

		case VUMA_VIDEO_RAM:
			loongson_sysconf.vram_type = VRAM_TYPE_UMA;
			loongson_sysconf.vuma_vram_addr = mem_start;
			loongson_sysconf.vuma_vram_size = mem_size << 20;
			break;
		case SYSTEM_RAM_LOW_DMA:
			pr_info("Debug: node_id:%d, mem_type:%d, mem_start:0x%llx, mem_size:0x%llx MB\n",
				(u32)node_id, mem_type, loongson_memmap->map[i].mem_start, mem_size);
			add_memory_region_dma(ls_dma_map, loongson_memmap->map[i].mem_start,
				(u64)loongson_memmap->map[i].mem_size << 20, SYSTEM_RAM_LOW_DMA, &dma_index);
			break;
		case SYSTEM_RAM_HIGH_DMA:
			pr_info("Debug: node_id:%d, mem_type:%d, mem_start:0x%llx, mem_size:0x%llx MB\n",
				(u32)node_id, mem_type, loongson_memmap->map[i].mem_start, mem_size);
			add_memory_region_dma(ls_dma_map, loongson_memmap->map[i].mem_start,
				(u64)loongson_memmap->map[i].mem_size << 20, SYSTEM_RAM_HIGH_DMA, &dma_index);
			break;
		}
	}
	reserve_video_ram(loongson_sysconf.uma_vram_addr, loongson_sysconf.uma_vram_size);
}

static void __init node_mem_init(unsigned int node)
{
	unsigned long node_addrspace_offset;
	unsigned long start_pfn, end_pfn;

	node_addrspace_offset = nid_to_addroffset(node);
	pr_info("Node%d's addrspace_offset is 0x%lx\n",
			node, node_addrspace_offset);

	get_pfn_range_for_nid(node, &start_pfn, &end_pfn);
	pr_info("Node%d: start_pfn=0x%lx, end_pfn=0x%lx\n",
		node, start_pfn, end_pfn);

	__node_data[node] = prealloc__node_data + node;

	NODE_DATA(node)->node_start_pfn = start_pfn;
	NODE_DATA(node)->node_spanned_pages = end_pfn - start_pfn;

	free_bootmem_with_active_regions(node, end_pfn);

	if (node == 0) {
		/* kernel start address */
		unsigned long kernel_start_pfn = PFN_DOWN(__pa_symbol(&_text));

		/* kernel end address */
		unsigned long kernel_end_pfn = PFN_UP(__pa_symbol(&_end));

		/* used by finalize_initrd() */
		max_low_pfn = end_pfn;

		/* Reserve the kernel text/data/bss */
		memblock_reserve(kernel_start_pfn << PAGE_SHIFT,
				 ((kernel_end_pfn - kernel_start_pfn) << PAGE_SHIFT));

		/* Reserve 0xfe000000~0xffffffff for RS780E integrated GPU */
		if (node_end_pfn(0) >= (0xffffffff >> PAGE_SHIFT))
			memblock_reserve((node_addrspace_offset | 0xfe000000),
					 32 << 20);
	}

	sparse_memory_present_with_active_regions(node);
}

static __init void prom_meminit(void)
{
	unsigned int node, cpu, active_cpu = 0;

	cpu_node_probe();
	init_topology_matrix();

	for (node = 0; node < loongson_sysconf.nr_nodes; node++) {
		if (node_online(node)) {
			szmem(node);
			node_mem_init(node);
			cpumask_clear(&__node_data[(node)]->cpumask);
		}
	}
	for (cpu = 0; cpu < loongson_sysconf.nr_cpus; cpu++) {
		node = cpu / loongson_sysconf.cores_per_node;
		if (node >= num_online_nodes())
			node = 0;

		if (loongson_sysconf.reserved_cpus_mask & (1<<cpu))
			continue;

		cpumask_set_cpu(active_cpu, &__node_data[(node)]->cpumask);
		pr_info("NUMA: set cpumask cpu %d on node %d\n", active_cpu, node);

		active_cpu++;
	}
}

#ifdef CONFIG_ACPI_NUMA

/*
 * Sanity check to catch more bad NUMA configurations (they are amazingly
 * common).  Make sure the nodes cover all memory.
 */
static bool __init numa_meminfo_cover_memory(const struct numa_meminfo *mi)
{
	u64 numaram, biosram;
	int i;

	numaram = 0;
	for (i = 0; i < mi->nr_blks; i++) {
		u64 s = mi->blk[i].start >> PAGE_SHIFT;
		u64 e = mi->blk[i].end >> PAGE_SHIFT;
		numaram += e - s;
		numaram -= __absent_pages_in_range(mi->blk[i].nid, s, e);
		if ((s64)numaram < 0)
			numaram = 0;
	}
	max_pfn = max_low_pfn;
	biosram = max_pfn - absent_pages_in_range(0, max_pfn);

	BUG_ON((s64)(biosram - numaram) >= (1 << (20 - PAGE_SHIFT)));
	return true;
}

static void add_node_intersection(u32 node, u64 start, u64 size)
{
	static unsigned long num_physpages = 0;
	num_physpages += (size >> PAGE_SHIFT);
	printk("Debug: node_id:%d, mem_start:0x%llx, mem_size:0x%llx Bytes\n",
		node, start, size);
	printk("       start_pfn:0x%llx, end_pfn:0x%llx, num_physpages:0x%lx\n",
		start >> PAGE_SHIFT, (start + size) >> PAGE_SHIFT, num_physpages);
	add_memory_region(start, size, BOOT_MEM_RAM);
	memblock_add_node(start, size, node);

}

/*
 * add_mem_region
 *
 * Add a uasable memory region described by BIOS. The
 * routine gets each intersection between BIOS's region
 * and node's region, and adds them into node's memblock
 * pool.
 *
 * */
static void __init add_mem_region(u64 start, u64 end)
{
	u32 i;
	u64 tmp = start;

	for (i = 0; i < numa_meminfo.nr_blks; i++) {
		struct numa_memblk *mb = &numa_meminfo.blk[i];

		if (tmp > mb->end)
			continue;

		if (end > mb->end) {
			add_node_intersection(mb->nid, tmp, mb->end - tmp);
			tmp = mb->end;
		} else {
			add_node_intersection(mb->nid, tmp, end - tmp);
			break;
		}
	}
}

extern unsigned long used_mem_item;
extern unsigned long used_mem_size;
static void __init init_node_memblock(void)
{
	u32 i, mem_type;
	u64 mem_end, mem_start, mem_size;

	/* Parse memory information and activate */
	for (i = 0; i < loongson_mem_map->map_count; i++) {

		mem_type = loongson_mem_map->map[i].mem_type;
		mem_start = loongson_mem_map->map[i].mem_start;
		mem_size = loongson_mem_map->map[i].mem_size;
		mem_end = loongson_mem_map->map[i].mem_start + mem_size;
		switch (mem_type) {
		case SYSTEM_RAM_LOW:
		case SYSTEM_RAM_HIGH:

			if (used_mem_item == i) mem_size += used_mem_size;
			memblock_remove(mem_start, mem_size);

			mem_start = PFN_ALIGN(mem_start);
			mem_end = PFN_ALIGN(mem_end - PAGE_SIZE + 1);
			if (mem_start >= mem_end)
				break;
			add_mem_region(mem_start, mem_end);
			break;

		case SYSTEM_RAM_RESERVED:
			printk("Debug: mem_type:%d, mem_start:0x%llx, mem_size:0x%llx Bytes\n",
					mem_type, mem_start, mem_size);
			add_memory_region(mem_start, mem_size, BOOT_MEM_RESERVED);
			memblock_reserve(mem_start, mem_size);
			break;
		case SMBIOS_TABLE:
			printk("Debug: mem_type:%d, mem_start:0x%llx, mem_size:0x%llx Bytes\n",
					mem_type, mem_start, mem_size);
			has_systab = 1;
			systab_addr = mem_start;
			add_memory_region(mem_start, mem_size, BOOT_MEM_RESERVED);
			memblock_reserve(mem_start, mem_size);
			break;
		case UMA_VIDEO_RAM:

			printk("Debug: mem_type:%d, mem_start:0x%llx, mem_size:0x%llx Bytes\n",
					mem_type, mem_start, mem_size);
			loongson_sysconf.vram_type = VRAM_TYPE_UMA;
			loongson_sysconf.uma_vram_addr = mem_start;
			loongson_sysconf.uma_vram_size = mem_size;
			break;
		case VUMA_VIDEO_RAM:

			printk("Debug: mem_type:%d, mem_start:0x%llx, mem_size:0x%llx Bytes\n",
					mem_type, mem_start, mem_size);
			loongson_sysconf.vuma_vram_addr = mem_start;
			loongson_sysconf.vuma_vram_size = mem_size;
			break;
		}
	}

	reserve_video_ram(loongson_sysconf.uma_vram_addr, loongson_sysconf.uma_vram_size);
}

static void __init numa_default_distance(void)
{
	int row, col;

	for (row = 0; row < MAX_NUMNODES; row++)
		for (col = 0; col < MAX_NUMNODES; col++) {

			if (col == row)
				__node_distances[row][col] = 0;
			else
				/* We assume that one node per package here!
				 *
				 * A SLIT should be used for multiple nodes per
				 * package to override default setting.
				 * */
				__node_distances[row][col] = 200;
	}
}
static int __init numa_mem_init(int (*init_func)(void))
{
	int i;
	int ret;
	int node;

	for (i = 0; i < CONFIG_NR_CPUS; i++)
		set_cpuid_to_node(i, NUMA_NO_NODE);
	nodes_clear(numa_nodes_parsed);
	nodes_clear(node_possible_map);
	nodes_clear(node_online_map);
	memset(&numa_meminfo, 0, sizeof(numa_meminfo));
	numa_default_distance();
	/* Parse SRAT and SLIT if provided by firmware. */
	ret = init_func();
	if (ret < 0)
		return ret;
	node_possible_map = numa_nodes_parsed;
	if (WARN_ON(nodes_empty(node_possible_map)))
		return -EINVAL;
	init_node_memblock();
	if (numa_meminfo_cover_memory(&numa_meminfo) == false) {
		return -EINVAL;
	}
	for_each_node_mask(node, node_possible_map) {
		node_mem_init(node);
		node_set_online(node);
		__node_data[(node)]->cpumask = cpus_on_node[node];
	}
	max_low_pfn = PHYS_PFN(memblock_end_of_DRAM());
	return 0;
}
#endif
void __init paging_init(void)
{
	unsigned node;
	unsigned long zones_size[MAX_NR_ZONES] = {0, };

	pagetable_init();

	for_each_online_node(node) {
		unsigned long  start_pfn, end_pfn;

		get_pfn_range_for_nid(node, &start_pfn, &end_pfn);

		if (end_pfn > max_low_pfn)
			max_low_pfn = end_pfn;
	}
#ifdef CONFIG_ZONE_DMA32
	zones_size[ZONE_DMA32] = MAX_DMA32_PFN;
#endif
	zones_size[ZONE_NORMAL] = max_low_pfn;
	free_area_init_nodes(zones_size);
}

void __init mem_init(void)
{
	high_memory = (void *) __va(get_num_physpages() << PAGE_SHIFT);
	free_all_bootmem();
	setup_zero_pages();	/* This comes from node 0 */
	mem_init_print_info(NULL);
}

/* All PCI device belongs to logical Node-0 */
int pcibus_to_node(struct pci_bus *bus)
{
	return 0;
}
EXPORT_SYMBOL(pcibus_to_node);

extern bool loongson_acpiboot_flag;
cpumask_t possible_cpu_per_node;
void __init prom_init_numa_memory(void)
{
	enable_lpa();

#ifndef CONFIG_ACPI_NUMA
	prom_meminit();
#else
	if (loongson_acpiboot_flag == false) {
		prom_meminit();
	} else {
		numa_mem_init(acpi_numa_init);
		setup_nr_node_ids();
		loongson_sysconf.nr_nodes = nr_node_ids;
		loongson_sysconf.cores_per_node = cpumask_weight(&cpus_on_node[0]);
	}
#endif
}
EXPORT_SYMBOL(prom_init_numa_memory);
