![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

# Memory Page Table Walker

This project explores how **virtual memory** is implemented in Linux by building page table walkers in both **user space** and **kernel space**. It was originally developed for an Operating Systems course, but is useful as a standalone reference for anyone learning about page tables, `/proc/self/pagemap`, `kmalloc`, and `vmalloc`.

The repo contains:

- A **user-space page table walker** (`memalloc`) that uses `/proc/self/pagemap` to translate user-level virtual addresses to physical addresses.
- A **kernel module** (`ko5204`) that allocates kernel memory using `vmalloc` and `kmalloc`, walks the kernel page tables, and logs the virtual→physical mappings.

---

## Project Structure

```text
.
├── user_space/                 # User-space page table walker
│   ├── memalloc.c
│   ├── pagemap.c
│   ├── pagemap.h
│   └── Makefile
└── kernel_space/                 # Kernel-space page table walker
    ├── ko5204.c
    └── Makefile
```

---

## User-Space Page Table Walker (`memalloc`)  

`memalloc` is a user-space program that:
1. Allocates memory in two different ways.
2. Walks the process's own page table via `/proc/self/pagemap`.
3. Prints one line per page in the form:
```bash
VA=0x<virtual_address> -> PA=0x<physical_address>
```
**This allows the user to see how Linux maps virtual pages to physical frames for a single process.**

### Allocation Modes
The user can choose the allocation strategy with `-b` and the **total size** with `-m`:
* `-b 4K` -> 4 KB mode
    * Allocates many **individual 4 KB blocks** (one per page) using a double-pointer array.
    * Each block is touched to ensure it is backed by a physical page.
    * Good for observing fragmented virtual allocations and scattered physical frames.

* `-b 1G` -> big block mode
    * Allocates a **single large contiguous virtual range**.
    * The total size is controlled by `-m` (does not have to be 1GiB; the name is historical)
    * Each page in that large block is touched and then translated.

**Note**
`-b` selects the **allocation mode** (many small blocks vs one big block).
`-m` selects the **total amount of memory** to allocate.

---

### Build
From `user_space/` directory, run:

```bash
make
```

To create the executable:

```bash
memalloc
```

---

### Usage

```bash
# Many 4KB blocks, 64MB total
./memalloc -b 4K -m 64M -o out_4k.txt

# One large block, 64MB total
./memalloc -b 1G -m 64M -o out_big.txt

# Full 1GB single block (if the machine can handle it)
./memalloc -b 1G -m 1G -o out_1g.txt
```

---

## Kernel-Space Page Table Walker (`ko5204`)

`ko5204` is a Linux Kernel module that:

* Creates a `/proc/pagetable` interface.
* Responds to simple commands written from user space:
    * `vmalloc`
    * `kmalloc`
* Allocated **1MiB** of kernel memory (256 x 4KiB pages), walks the mapping page-by-page and logs:

```bash
Vmalloc: VA=0xffff89af05cf8000 -> PA=0x0000000123456000
Kmalloc: VA=0xffff89af05cf9000 -> PA=0x0000000123457000
```

These logs show how the kernel maps its own virtual addresses to physical frames for different allocators.

---

### Analysis vmalloc vs kmalloc

*`vmalloc`
    * Returns a **virtually contiguous** region.
    * Backed by **non-contiguous** physical pages.
    * We use `vmalloc_to_page()` + `page_to_phys()` to translate VA -> PA.
    * Physical addresses jump around between pages.

*`kmalloc`
    * Returns **physically contiguous memory (from direct-mapped region).
    * We use `virt_to_phys()` to translate VA -> PA.
    * Both virtually and physical addresses usually increase smoothly page-by-page.
    * Large allocations can fail if there isn't enough contiguous physical memory.

---

### Build
From `kernel_space/` directory, run:

```bash
sudo apt-get update
sudo apt-get install -y build-essential linux-headers-$(uname -r)

make
```

To create the executable:

```bash
ko5204.ko
```

---

### Load and Use

```bash

# Insert the module
sudo insmod ko5204.ko

# Confirm it's loaded
lsmod | grep ko5204

# Trigger vmalloc-based walk
echo "vmalloc" | sudo tee /proc/pagetable

# Trigger kmalloc-based walk
echo "kmalloc" | sudo tee /proc/pagetable
```

Check the kernel logs:
```bash
sudo dmesg | tail -n 100

# or on Ubuntu:

sudo tail -n 100 /var/log/kern.log
```

Unload the module when done:
```bash
sudo rmmod ko5204
```

---

## License

This project is released under the **MIT License**.

---

## Author
Felipe Campoverde


