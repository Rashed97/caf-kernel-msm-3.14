/*
 * Definitions for working with the Flattened Device Tree data format
 *
 * Copyright 2009 Benjamin Herrenschmidt, IBM Corp
 * benh@kernel.crashing.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef _LINUX_OF_FDT_H
#define _LINUX_OF_FDT_H

#include <linux/types.h>
#include <linux/init.h>

/* Definitions used by the flattened device tree */
#define OF_DT_HEADER		0xd00dfeed	/* marker */
#define OF_DT_BEGIN_NODE	0x1		/* Start of node, full name */
#define OF_DT_END_NODE		0x2		/* End node */
#define OF_DT_PROP		0x3		/* Property: name off, size,
						 * content */
#define OF_DT_NOP		0x4		/* nop */
#define OF_DT_END		0x9

#define OF_DT_VERSION		0x10

#ifndef __ASSEMBLY__
/*
 * This is what gets passed to the kernel by prom_init or kexec
 *
 * The dt struct contains the device tree structure, full pathes and
 * property contents. The dt strings contain a separate block with just
 * the strings for the property names, and is fully page aligned and
 * self contained in a page, so that it can be kept around by the kernel,
 * each property name appears only once in this page (cheap compression)
 *
 * the mem_rsvmap contains a map of reserved ranges of physical memory,
 * passing it here instead of in the device-tree itself greatly simplifies
 * the job of everybody. It's just a list of u64 pairs (base/size) that
 * ends when size is 0
 */
struct boot_param_header {
	__be32	magic;			/* magic word OF_DT_HEADER */
	__be32	totalsize;		/* total size of DT block */
	__be32	off_dt_struct;		/* offset to structure */
	__be32	off_dt_strings;		/* offset to strings */
	__be32	off_mem_rsvmap;		/* offset to memory reserve map */
	__be32	version;		/* format version */
	__be32	last_comp_version;	/* last compatible version */
	/* version 2 fields below */
	__be32	boot_cpuid_phys;	/* Physical CPU id we're booting on */
	/* version 3 fields below */
	__be32	dt_strings_size;	/* size of the DT strings block */
	/* version 17 fields below */
	__be32	dt_struct_size;		/* size of the DT structure block */
};

#if defined(CONFIG_OF_FLATTREE)

struct device_node;

/* For scanning an arbitrary device-tree at any time */
extern char *of_fdt_get_string(const void *blob, u32 offset);
extern void *of_fdt_get_property(const void *blob,
				 unsigned long node,
				 const char *name,
				 int *size);
extern int of_fdt_is_compatible(const void *blob,
				unsigned long node,
				const char *compat);
extern int of_fdt_match(const void *blob, unsigned long node,
			const char *const *compat);
extern void of_fdt_unflatten_tree(unsigned long *blob,
			       struct device_node **mynodes);

/* TBD: Temporary export of fdt globals - remove when code fully merged */
extern int __initdata dt_root_addr_cells;
extern int __initdata dt_root_size_cells;
extern void *initial_boot_params;

/* For scanning the flat device-tree at boot time */
extern int of_scan_flat_dt(int (*it)(unsigned long node, const char *uname,
				     int depth, void *data),
			   void *data);
extern const void *of_get_flat_dt_prop(unsigned long node, const char *name,
				       int *size);
extern int of_flat_dt_is_compatible(unsigned long node, const char *name);
extern int of_flat_dt_match(unsigned long node, const char *const *matches);
extern unsigned long of_get_flat_dt_root(void);
extern int of_get_flat_dt_size(void);
extern int of_scan_flat_dt_by_path(const char *path,
	int (*it)(unsigned long node, const char *name, int depth, void *data),
	void *data);

extern int early_init_dt_scan_chosen(unsigned long node, const char *uname,
				     int depth, void *data);
extern int early_init_dt_scan_memory(unsigned long node, const char *uname,
				     int depth, void *data);
extern void early_init_fdt_scan_reserved_mem(void);
extern void early_init_dt_add_memory_arch(u64 base, u64 size);
extern int early_init_dt_reserve_memory_arch(phys_addr_t base, phys_addr_t size,
					     bool no_map);
extern void * early_init_dt_alloc_memory_arch(u64 size, u64 align);
extern u64 dt_mem_next_cell(int s, const __be32 **cellp);

/* Early flat tree scan hooks */
extern int early_init_dt_scan_root(unsigned long node, const char *uname,
				   int depth, void *data);

extern bool early_init_dt_scan(void *params);

extern const char *of_flat_dt_get_machine_name(void);
extern const void *of_flat_dt_match_machine(const void *default_match,
		const void * (*get_next_compat)(const char * const**));

/* Other Prototypes */
extern void unflatten_device_tree(void);
extern void unflatten_and_copy_device_tree(void);
extern void early_init_devtree(void *);
extern void early_get_first_memblock_info(void *, phys_addr_t *);
#else /* CONFIG_OF_FLATTREE */
static inline void early_init_fdt_scan_reserved_mem(void) {}
static inline const char *of_flat_dt_get_machine_name(void) { return NULL; }
static inline void unflatten_device_tree(void) {}
static inline void unflatten_and_copy_device_tree(void) {}
#endif /* CONFIG_OF_FLATTREE */

#endif /* __ASSEMBLY__ */
#endif /* _LINUX_OF_FDT_H */
