/*
 * ko5204.c - CS5204 Task 3 kernel page-table walker
 * -------------------------------------------------
 *
 * Usage (as root):
 * 	insmod ko5204.ko
 * 	echo "vmalloc" > /proc/pagetable
 * 	echo "kmalloc" > /proc/pagetable
 * 	dmegs | tail -n 100	$ or tail /var/log/kern.log
 *
 * Logs one line per 4KiB page, Example:
 * 	Vmalloc: VA=0xffff88810a100000 -> PA=0x0000000123456000
 * 	Kmalloc: VA=0xffff888109000000 -> PA=0x0000000456789000
 *
 * 	( could have forgotten a digit somewhere )
 *
 * Build deps: linux-headers-$(uname -r)
 *
 * For each case, it allocates 1MiB of kernel memory (256 pages * 4KB), walks the region page-by-page, and logs each virtual-to-physical address translation to the kernel log (visible via `dmesg` or `/var/log/kern.log`).
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>


#include <linux/io.h>
#include <linux/vmalloc.h>

#include <linux/proc_fs.h>	// for proc_create(), proc_remove()
#include <linux/uaccess.h>	// for copy_from_user()
#include <linux/slab.h>		// for kmalloc/kfree
#include <linux/mm.h>		// for page_to_phys(), PAGE_SIZE
#include <linux/version.h>	// for LINUX_VERSION_CODE checks

#define PROC_NAME	"pagetable"		// Proc entry name (/proc/cs5204)
#define REQ_PAGES	256ul			// 256 * 4KiB  = 1MiB
#define REGION_SZ	(REQ_PAGES * PAGE_SIZE)	// total region size in bytes


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Felipe Campoverde");
MODULE_DESCRIPTION("Simple Kernel Page Table Walker");
MODULE_VERSION("0.1");


static struct proc_dir_entry *proc_entry;

/* ============================================================================
 * Helper: Walk vmalloc region page-by-page and log VA -> PA mapping
 * ============================================================================ */

static void walk_vmalloc_region(void *base) {
	unsigned long off;

	for (off = 0; off < REGION_SZ; off += PAGE_SIZE) {
		// Computer the current virtual address
		void *va = (void *)((unsigned long) base + off);

		// Retrieve the struct page corresponding to the virtual address.
		// vmalloc regions are *virtually contiguous* but *physically scattered*
		struct page *pg = vmalloc_to_page(va);

		if (!pg) {
			pr_info("Vmalloc: VA=0x%px -> PA=<unmapped>\n", va);
			continue;
		}

		// Base PFN -> physical address of the page, then add offset within page

		{
			// Translate struct page to physical address (frame base) and add the offset
			// within the page to get the exact PA
			phys_addr_t pa = page_to_phys(pg) + offset_in_page((unsigned long)va);
			pr_info("Vmalloc: VA=0x%px -> PA=0x%llx\n", va, (unsigned long long)pa);
		}
	}
}


/* =============================================================================
 * Helper: walk kmalloc region page-by-page and log va -> pa mapping
 * ============================================================================= */

static void walk_kmalloc_region(void *base) {
	unsigned long off;

	for (off = 0; off < REGION_SZ; off += PAGE_SIZE) {
		void *va = (void *)((unsigned long)base + off);

		// kmalloc memory lives in the direct map on typical configs
		// so virt_to_phys() yields the correct PA.
		{
			phys_addr_t pa = virt_to_phys(va);
			pr_info("Kmalloc: VA=0x%px -> PA=0x%llx\n", va, (unsigned long long)pa);
		}
	}
}

/* =============================================================================
 * /proc write handler - handles user input like "vmalloc" or "kmalloc"
 * ============================================================================= */

static ssize_t cs5204_proc_write(struct file *file,
				 const char __user *ubuf,
				 size_t len, loff_t *ppos) {
	char cmd[32];

	// safety check for preventing empty writes
	if (len == 0) {
		return 0;
	}

	// Cap length to buffer
	if (len >= sizeof(cmd)) {
		len = sizeof(cmd) - 1;
	}

	// Copy command from user-space buffer
	if (copy_from_user(cmd, ubuf, len)) {
		return -EFAULT;
	}

	cmd[len] = '\0';

	// Strip trailing whitespace / newlines
	
	{
		char *p = cmd + strlen(cmd);
		while (p > cmd && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ' || p[-1] == '\t'))
			*--p = '\0';
	}

	/* --------------------------------------------------------
	 * Case 1: VMALLOC
	 * -------------------------------------------------------- */
	if (!strcmp(cmd, "vmalloc")) {
		void *buf = vmalloc(REGION_SZ);
		if (!buf) {
			pr_err("cs5204: vmalloc(%lu) failed\n", (unsigned long)REGION_SZ);
			return -ENOMEM;
		}

		// Touch to ensure mappings exist
		memset(buf, 0xA5, REGION_SZ);

		pr_info("cs5204: walking vmalloc region %p - %p\n", buf, (void *)((unsigned long)buf + REGION_SZ - 1));
		walk_vmalloc_region(buf);

		vfree(buf);
		pr_info("cs5204: vmalloc region freed\n");

	
	/* --------------------------------------------------------
	 * Case 2: KMALLOC
	 * -------------------------------------------------------- */
	} else if (!strcmp(cmd, "kmalloc")) {
		void *buf = kmalloc(REGION_SZ, GFP_KERNEL);
		if (!buf) {
			pr_err("cs5204: kmalloc(%lu) failed (physically contiguous; may be fragmented)\n", (unsigned long)REGION_SZ);
			return -ENOMEM;
		}

		memset(buf, 0x5A, REGION_SZ);

		pr_info("cs5204: walking kmalloc region %p - %p\n", buf, (void *)((unsigned long) buf + REGION_SZ - 1));
		walk_kmalloc_region(buf);

		kfree(buf);
		pr_info("cs5204: kmalloc region freed\n");

	
	/* --------------------------------------------------------
	 * Invalid input
	 * -------------------------------------------------------- */
	} else {
		pr_info("cs5204: unknown command '%s' (use 'vmalloc' or 'kmalloc')\n", cmd);
		return -EINVAL;
	}

	*ppos += len;	// Advance file position
	return len;
}


/* =============================================================================
 * Define /proc file operations (depending on kernel version)
 * ============================================================================= */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops cs5204_proc_ops = {
	.proc_write = cs5204_proc_write,
};
#else
static const struct file_operations_cs5204_proc_ops = {
	.owner = THIS_MODULE,
	.write = cs5204_proc_write,
};
#endif


/* =============================================================================
 *  Module initialization and cleanup
 * ============================================================================= */

static int __init pagetable_walker_init(void) {

	// Create writable /proc entry
	proc_entry = proc_create(PROC_NAME, 0222, NULL, &cs5204_proc_ops);
	if (!proc_entry) {
		pr_err("cs5204: failed to create /proc/%s\n", PROC_NAME);
		return -ENOMEM;
	}
	pr_info("cs5204: /proc/%s ready (echo 'vmalloc' or 'kmalloc')\n", PROC_NAME);
	return 0;
}

static void __exit pagetable_walker_exit(void) {

	// Remoe /proc entry on unload
	if (proc_entry)
		proc_remove(proc_entry);
	pr_info("cs5204: module unloaded\n");
}

module_init(pagetable_walker_init);
module_exit(pagetable_walker_exit);


