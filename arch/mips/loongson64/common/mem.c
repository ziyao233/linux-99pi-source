/*
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/mm.h>

#include <asm/bootinfo.h>

#include <loongson.h>
#include <boot_param.h>
#include <mem.h>
#include <pci.h>

extern struct loongsonlist_mem_map *loongson_mem_map;

#ifndef CONFIG_LEFI_FIRMWARE_INTERFACE

u32 memsize, highmemsize;

void __init prom_init_memory_new(void)
{
	int i;
	u64 mem_start, mem_end, mem_size;

	/* parse memory information */
	for (i = 0; i < loongson_mem_map->map_count; i++){
		mem_type = loongson_mem_map->map[i].mem_type;
		mem_start = loongson_mem_map->map[i].mem_start;
		mem_size = loongson_mem_map->map[i].mem_size;
		mem_end = mem_start + mem_size;
		switch (mem_type) {
		case SYSTEM_RAM_LOW:
			mem_start = PFN_ALIGN(mem_start);
			mem_end = PFN_ALIGN(mem_end - PAGE_SIZE + 1);
			if (mem_start >= mem_end)
				break;
			low_physmem_start = loongson_mem_map->map[i].mem_start;
			add_memory_region(loongson_mem_map->map[i].mem_start,
				loongson_mem_map->map[i].mem_size,
				BOOT_MEM_RAM);
			break;
		case SYSTEM_RAM_HIGH:
			mem_start = PFN_ALIGN(mem_start);
			mem_end = PFN_ALIGN(mem_end - PAGE_SIZE + 1);
			if (mem_start >= mem_end)
				break;
			high_physmem_start = loongson_mem_map->map[i].mem_start;
			add_memory_region(loongson_mem_map->map[i].mem_start,
				loongson_mem_map->map[i].mem_size,
				BOOT_MEM_RAM);
			break;
		case MEM_RESERVED:
			add_memory_region(loongson_mem_map->map[i].mem_start,
				loongson_mem_map->map[i].mem_size,
				BOOT_MEM_RESERVED);
			break;
		case SMBIOS_TABLE:
			has_systab = 1;
			systab_addr = loongson_mem_map->map[i].mem_start;
			add_memory_region(loongson_mem_map->map[i].mem_start,
				loongson_mem_map->map[i].mem_size, BOOT_MEM_RESERVED);
			break;
		case UMA_VIDEO_RAM:
			vram_type = VRAM_TYPE_UMA;
			uma_vram_addr = loongson_mem_map->map[i].mem_start & 0xffffffff;
			uma_vram_size = loongson_mem_map->map[i].mem_size;
			add_memory_region(loongson_mem_map->map[i].mem_start,
				loongson_mem_map->map[i].mem_size,
				BOOT_MEM_RESERVED);
			break;
		}
	}
}
void __init prom_init_memory(void)
{
	add_memory_region(0x0, (memsize << 20), BOOT_MEM_RAM);

	add_memory_region(memsize << 20, LOONGSON_PCI_MEM_START - (memsize <<
				20), BOOT_MEM_RESERVED);

#ifdef CONFIG_CPU_SUPPORTS_ADDRWINCFG
	{
		int bit;

		bit = fls(memsize + highmemsize);
		if (bit != ffs(memsize + highmemsize))
			bit += 20;
		else
			bit = bit + 20 - 1;

		/* set cpu window3 to map CPU to DDR: 2G -> 2G */
		LOONGSON_ADDRWIN_CPUTODDR(ADDRWIN_WIN3, 0x80000000ul,
					  0x80000000ul, (1 << bit));
		mmiowb();
	}
#endif /* !CONFIG_CPU_SUPPORTS_ADDRWINCFG */

#ifdef CONFIG_64BIT
	if (highmemsize > 0)
		add_memory_region(LOONGSON_HIGHMEM_START,
				  highmemsize << 20, BOOT_MEM_RAM);

	add_memory_region(LOONGSON_PCI_MEM_END + 1, LOONGSON_HIGHMEM_START -
			  LOONGSON_PCI_MEM_END - 1, BOOT_MEM_RESERVED);

#endif /* !CONFIG_64BIT */
}

#else /* CONFIG_LEFI_FIRMWARE_INTERFACE */

extern unsigned int has_systab;
extern unsigned long systab_addr;

#include <linux/bootmem.h>
#include <linux/memblock.h>
void __init memblock_and_maxpfn_init(void)
{
	int i;
	u32 mem_type;
	u64 mem_start, mem_end, mem_size;
	/* parse memory information */
	for (i = 0; i < loongson_mem_map->map_count; i++){

		mem_type = loongson_mem_map->map[i].mem_type;
		mem_start = loongson_mem_map->map[i].mem_start;
		mem_size = loongson_mem_map->map[i].mem_size;
		mem_end = mem_start + mem_size;
		switch (mem_type) {
		case SYSTEM_RAM_LOW:
			max_low_pfn_mapped = mem_end  >> PAGE_SHIFT;
		case SYSTEM_RAM_HIGH:
			memblock_add(mem_start, mem_size);
			if (max_low_pfn < (mem_end >> PAGE_SHIFT))
				max_low_pfn = mem_end >> PAGE_SHIFT;
			break;
		}
	}
	memblock_set_current_limit(PFN_PHYS(max_low_pfn));
}

void __init prom_init_memory(void)
{
	int i;
	u32 node_id;
	u32 mem_type;

	/* parse memory information */
	for (i = 0; i < loongson_memmap->nr_map; i++) {
		node_id = loongson_memmap->map[i].node_id;
		mem_type = loongson_memmap->map[i].mem_type;

		if (node_id == 0) {
			switch (mem_type) {
			case SYSTEM_RAM_LOW:
				loongson_sysconf.low_physmem_start =
					loongson_memmap->map[i].mem_start;
				add_memory_region(loongson_memmap->map[i].mem_start,
					(u64)loongson_memmap->map[i].mem_size << 20,
					BOOT_MEM_RAM);
				break;
			case SYSTEM_RAM_HIGH:
				loongson_sysconf.high_physmem_start =
					loongson_memmap->map[i].mem_start;
				add_memory_region(loongson_memmap->map[i].mem_start,
					(u64)loongson_memmap->map[i].mem_size << 20,
					BOOT_MEM_RAM);
				break;
			case SYSTEM_RAM_RESERVED:
				add_memory_region(loongson_memmap->map[i].mem_start,
					(u64)loongson_memmap->map[i].mem_size << 20,
					BOOT_MEM_RESERVED);
				break;
			case SMBIOS_TABLE:
				has_systab = 1;
				systab_addr = loongson_memmap->map[i].mem_start;
				add_memory_region(loongson_memmap->map[i].mem_start,
					0x2000, BOOT_MEM_RESERVED);
				break;
			case UMA_VIDEO_RAM:
				loongson_sysconf.vram_type = VRAM_TYPE_UMA;
				loongson_sysconf.uma_vram_addr = loongson_memmap->map[i].mem_start;
				loongson_sysconf.uma_vram_size = loongson_memmap->map[i].mem_size << 20;
				add_memory_region(loongson_memmap->map[i].mem_start,
					(u64)loongson_memmap->map[i].mem_size << 20,
					BOOT_MEM_RESERVED);
				break;
			case VUMA_VIDEO_RAM:
				loongson_sysconf.vram_type = VRAM_TYPE_UMA;
				loongson_sysconf.vuma_vram_addr = loongson_memmap->map[i].mem_start;
				loongson_sysconf.vuma_vram_size = loongson_memmap->map[i].mem_size << 20;
				add_memory_region(loongson_memmap->map[i].mem_start,
					(u64)loongson_memmap->map[i].mem_size << 20,
					BOOT_MEM_RESERVED);
				break;
			}
		}
	}
}

#endif /* CONFIG_LEFI_FIRMWARE_INTERFACE */

/* override of arch/mips/mm/cache.c: __uncached_access */
int __uncached_access(struct file *file, unsigned long addr)
{
	if (file->f_flags & O_DSYNC)
		return 1;

	return addr >= __pa(high_memory) ||
		((addr >= LOONGSON_MMIO_MEM_START) &&
		 (addr < LOONGSON_MMIO_MEM_END));
}

#ifdef CONFIG_CPU_SUPPORTS_UNCACHED_ACCELERATED

#include <linux/pci.h>
#include <linux/sched.h>
#include <asm/current.h>

static unsigned long uca_start, uca_end;

pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
			      unsigned long size, pgprot_t vma_prot)
{
	unsigned long offset = pfn << PAGE_SHIFT;
	unsigned long end = offset + size;

	if (__uncached_access(file, offset)) {
		if (uca_start && (offset >= uca_start) &&
		    (end <= uca_end))
			return __pgprot((pgprot_val(vma_prot) &
					 ~_CACHE_MASK) |
					_CACHE_UNCACHED_ACCELERATED);
		else
			return pgprot_noncached(vma_prot);
	}
	return vma_prot;
}

static int __init find_vga_mem_init(void)
{
	struct pci_dev *dev = 0;
	struct resource *r;
	int idx;

	if (uca_start)
		return 0;

	for_each_pci_dev(dev) {
		if ((dev->class >> 16) == PCI_BASE_CLASS_DISPLAY) {
			for (idx = 0; idx < PCI_NUM_RESOURCES; idx++) {
				r = &dev->resource[idx];
				if (!r->start && r->end)
					continue;
				if (r->flags & IORESOURCE_IO)
					continue;
				if (r->flags & IORESOURCE_MEM) {
					uca_start = r->start;
					uca_end = r->end;
					return 0;
				}
			}
		}
	}

	return 0;
}

late_initcall(find_vga_mem_init);
#endif /* !CONFIG_CPU_SUPPORTS_UNCACHED_ACCELERATED */
