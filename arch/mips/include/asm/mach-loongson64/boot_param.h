/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/screen_info.h>
#ifndef __ASM_MACH_LOONGSON64_BOOT_PARAM_H_
#define __ASM_MACH_LOONGSON64_BOOT_PARAM_H_

#define SYSTEM_RAM_LOW		1
#define SYSTEM_RAM_HIGH		2
#define SYSTEM_RAM_RESERVED	3
#define PCI_IO			4
#define PCI_MEM			5
#define LOONGSON_CFG_REG	6
#define VIDEO_ROM		7
#define ADAPTER_ROM		8
#define ACPI_TABLE		9
#define SMBIOS_TABLE		10
#define UMA_VIDEO_RAM		11
#define VUMA_VIDEO_RAM		12
#define SYSTEM_RAM_LOW_DMA      13
#define SYSTEM_RAM_HIGH_DMA     14
#define MAX_MEMORY_TYPE		15

#define VRAM_TYPE_SP	0
#define VRAM_TYPE_UMA	1

#define LOONGSON3_BOOT_MEM_MAP_MAX 128
#define LOONGSON3_BOOT_MEM_MAP 64

struct efi_memory_map_loongson {
	u16 vers;	/* version of efi_memory_map */
	u32 nr_map;	/* number of memory_maps */
	u32 mem_freq;	/* memory frequence */
	struct mem_map {
		u32 node_id;	/* node_id which memory attached to */
		u32 mem_type;	/* system memory, pci memory, pci io, etc. */
		u64 mem_start;	/* memory map start address */
		u32 mem_size;	/* each memory_map size, not the total size */
	} map[LOONGSON3_BOOT_MEM_MAP_MAX];
} __packed;

struct dma_mem_map{
    u32 mem_type;
    u64 mem_start;
    u64 mem_size;
}__attribute__((packed));

enum loongson_cpu_type {
	Legacy_2E = 0x0,
	Legacy_2F = 0x1,
	Legacy_3A = 0x2,
	Legacy_3B = 0x3,
	Legacy_1A = 0x4,
	Legacy_1B = 0x5,
	Legacy_2G = 0x6,
	Legacy_2H = 0x7,
	Loongson_1A = 0x100,
	Loongson_1B = 0x101,
	Loongson_2E = 0x200,
	Loongson_2F = 0x201,
	Loongson_2G = 0x202,
	Loongson_2H = 0x203,
	Loongson_3A = 0x300,
	Loongson_3B = 0x301
};

/*
 * Capability and feature descriptor structure for MIPS CPU
 */
struct efi_cpuinfo_loongson {
	u16 vers;     /* version of efi_cpuinfo_loongson */
	u32 processor_id; /* PRID, e.g. 6305, 6306 */
	u32 cputype;  /* Loongson_3A/3B, etc. */
	u32 total_node;   /* num of total numa nodes */
	u16 cpu_startup_core_id; /* Boot core id */
	u16 reserved_cores_mask;
	u32 cpu_clock_freq; /* cpu_clock */
	u32 nr_cpus;
	char cpuname[64];
} __packed;

#define MAX_UARTS 64
struct uart_device {
	u32 iotype; /* see include/linux/serial_core.h */
	u32 uartclk;
	u32 int_offset;
	u64 uart_base;
} __packed;

#define MAX_SENSORS 64
#define SENSOR_TEMPER  0x00000001
#define SENSOR_VOLTAGE 0x00000002
#define SENSOR_FAN     0x00000004
struct sensor_device {
	char name[32];  /* a formal name */
	char label[64]; /* a flexible description */
	u32 type;       /* SENSOR_* */
	u32 id;         /* instance id of a sensor-class */
	u32 fan_policy; /* see loongson_hwmon.h */
	u32 fan_percent;/* only for constant speed policy */
	u64 base_addr;  /* base address of device registers */
} __packed;

struct system_loongson {
	u16 vers;     /* version of system_loongson */
	u32 ccnuma_smp; /* 0: no numa; 1: has numa */
	u32 sing_double_channel; /* 1:single; 2:double */
	u32 nr_uarts;
	struct uart_device uarts[MAX_UARTS];
	u32 nr_sensors;
	struct sensor_device sensors[MAX_SENSORS];
	char has_ec;
	char ec_name[32];
	u64 ec_base_addr;
	char has_tcm;
	char tcm_name[32];
	u64 tcm_base_addr;
	u64 workarounds; /* see workarounds.h */
	u64 of_dtb_addr; /* NULL if not support */
} __packed;

struct irq_source_routing_table {
	u16 vers;
	u16 size;
	u16 rtr_bus;
	u16 rtr_devfn;
	u32 vendor;
	u32 device;
	u32 PIC_type;   /* conform use HT or PCI to route to CPU-PIC */
	u64 ht_int_bit; /* 3A: 1<<24; 3B: 1<<16 */
	u64 ht_enable;  /* irqs used in this PIC */
	u32 node_id;    /* node id: 0x0-0; 0x1-1; 0x10-2; 0x11-3 */
	u64 pci_mem_start_addr;
	u64 pci_mem_end_addr;
	u64 pci_io_start_addr;
	u64 pci_io_end_addr;
	u64 pci_config_addr;
	u16 dma_mask_bits;
	u16 dma_noncoherent;
} __packed;

struct interface_info {
	union {
		u16 vers; /* version of the specificition */
		struct {
			u8 minor;
			u8 major;
		}version;
	}__packed;
	u16 size;
	u8  flag;
	char description[64];
} __packed;

#define MAX_RESOURCE_NUMBER 128
struct resource_loongson {
	u64 start; /* resource start address */
	u64 end;   /* resource end address */
	char name[64];
	u32 flags;
};

struct archdev_data {};  /* arch specific additions */

struct board_devices {
	char name[64];    /* hold the device name */
	u32 num_resources; /* number of device_resource */
	/* for each device's resource */
	struct resource_loongson resource[MAX_RESOURCE_NUMBER];
	/* arch specific additions */
	struct archdev_data archdata;
};

struct loongson_special_attribute {
	u16 vers;     /* version of this special */
	char special_name[64]; /* special_atribute_name */
	u32 loongson_special_type; /* type of special device */
	/* for each device's resource */
	struct resource_loongson resource[MAX_RESOURCE_NUMBER];
};

struct loongson_params {
	u64 memory_offset;	/* efi_memory_map_loongson struct offset */
	u64 cpu_offset;		/* efi_cpuinfo_loongson struct offset */
	u64 system_offset;	/* system_loongson struct offset */
	u64 irq_offset;		/* irq_source_routing_table struct offset */
	u64 interface_offset;	/* interface_info struct offset */
	u64 special_offset;	/* loongson_special_attribute struct offset */
	u64 boarddev_table_offset;  /* board_devices offset */
};

struct sysinfo_tables {
	u16 vers;     /* version of sysinfo */
	u64 vga_bios; /* vga_bios address */
	struct loongson_params lp;
};

struct efi_reset_system_t {
	u64 ResetCold;
	u64 ResetWarm;
	u64 ResetType;
	u64 Shutdown;
	u64 DoSuspend; /* NULL if not support */
};

struct efi_loongson {
	u64 mps;	/* MPS table */
	u64 acpi;	/* ACPI table (IA64 ext 0.71) */
	u64 acpi20;	/* ACPI table (ACPI 2.0) */
	struct sysinfo_tables sysinfo;	/* Sysinfo table */
	u64 sal_systab;	/* SAL system table */
	u64 boot_info;	/* boot info table */
};

struct boot_params {
	struct efi_loongson efi;
	struct efi_reset_system_t reset_system;
};

struct loongson_system_configuration {
	u32 nr_cpus;
	u32 nr_nodes;
	int cores_per_node;
	int cores_per_package;
	u16 boot_cpu_id;
	u16 reserved_cpus_mask;
	enum loongson_cpu_type cputype;
	u64 ht_control_base;
	u64 pci_mem_start_addr;
	u64 pci_mem_end_addr;
	u64 pci_io_base;
	u64 restart_addr;
	u64 poweroff_addr;
	u64 suspend_addr;
	u64 vgabios_addr;
	u64 low_physmem_start;
	u64 high_physmem_start;
	u32 dma_mask_bits;
	u32 vram_type;
	u64 uma_vram_addr;
	u64 uma_vram_size;
	u64 vuma_vram_addr;
	u64 vuma_vram_size;
	u32 ec_sci_irq;
	char ecname[32];
	u32 nr_uarts;
	struct uart_device uarts[MAX_UARTS];
	u32 nr_sensors;
	struct sensor_device sensors[MAX_SENSORS];
	u64 workarounds;
	u32 msi_address_lo;
	u32 msi_address_hi;
	u32 msi_base_irq;
	u32 msi_last_irq;
	u32 io_base_irq;
	u32 io_last_irq;
};


#define PCI_MEM_START_ADDR		0x40000000
#define PCI_MEM_END_ADDR		0x7fffffff
#define LOONGSON_PCI_IOBASE		0xefdfc000000
#define LOONGSON_DMA_MASK_BIT		64
#define LOONGSON_MEM_LINKLIST		"MEM"
#define LOONGSON_VBIOS_LINKLIST		"VBIOS"
#define LOONGSON_EFIBOOT_SIGNATURE	"BPI"
#define LOONGSON_SCREENINFO_LINKLIST	"SINFO"

struct bootparamsinterface {
	u64	signature;	/*{"B", "P", "I", "_", "0", "_", "1"}*/
	void	*systemtable;
	struct	_extention_list_hdr	*extlist;
}__attribute__((packed));

struct _extention_list_hdr {
	u64	signature;
	u32	length;
	u8	revision;
	u8	checksum;
	struct	_extention_list_hdr *next;
}__attribute__((packed));

struct loongsonlist_mem_map {
	struct	_extention_list_hdr header;	/*{"M", "E", "M"}*/
	u8	map_count;
	struct	_loongson_mem_map {
		u32 mem_type;
		u64 mem_start;
		u64 mem_size;
	}__attribute__((packed))map[LOONGSON3_BOOT_MEM_MAP_MAX];
}__attribute__((packed));

struct loongsonlist_vbios {
	struct	_extention_list_hdr header;
	u64	vbios_addr;
}__attribute__((packed));

struct loongsonlist_screeninfo{

	struct	_extention_list_hdr header;
	struct	screen_info si;
};

extern void *loongson_fdt_blob;
extern u32 __dtb_loongson3_ls2h_begin[];
extern u32 __dtb_loongson3_ls7a_begin[];
extern u32 __dtb_loongson3_rs780_begin[];
extern struct efi_memory_map_loongson *loongson_memmap;
extern struct loongson_system_configuration loongson_sysconf;
extern struct dma_mem_map ls_dma_map[LOONGSON3_BOOT_MEM_MAP];
extern struct dma_mem_map ls_phy_map[LOONGSON3_BOOT_MEM_MAP];
#endif
