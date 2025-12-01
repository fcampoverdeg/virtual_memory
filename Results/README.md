## 📘 Project Summary & Conclusions

This project explored how Linux manages virtual memory through both user-space and kernel-space page table walking. By implementing two independent walkers — one using `/proc/self/pagemap` and another implemented as a kernel module — it becomes clear how virtual addresses are mapped, how physical memory is allocated, and how user-level vs kernel-level memory management differ.

---

### 🔹 Background Study

A **multi-level page table** is a hierarchical structure that maps virtual pages to physical frames. Instead of storing one huge table, the OS divides the mapping into several smaller tables (e.g., 4 levels on x86-64). Only the necessary parts of the hierarchy are created, saving memory while still supporting fast lookups.

In **user space**, each process has its own private virtual address space. Processes cannot modify their page tables directly, but they can inspect mappings through the `/proc/<pid>/pagemap` interface.  
In **kernel space**, the kernel has full control of page tables and can allocate memory using mechanisms such as `kmalloc` (physically contiguous) and `vmalloc` (virtually contiguous). These mechanisms directly influence how virtual-to-physical translations behave.

---

### 🔹 User-Space Observations

#### **Are virtual and physical addresses continuous?**
- **Virtual addresses**: Yes, they appear continuous within each allocated region.
- **Physical addresses**: No. Physical frames are assigned wherever space is available.
  
Only the **4 KB page offset** remains identical between VA and PA (lower 12 bits).

#### **Is the offset fixed?**
No. Each page may be placed in a different physical frame, so there is no global offset.

#### **Does the offset match the size passed to `calloc`?**
No. `calloc` returns virtual memory that is not tied to physical layout. Demand paging, fragmentation, and the kernel’s allocator all influence PA selection.

#### **Differences between 4 KB mode and 1 GB mode**
- **4 KB mode**: Many small allocations → very fragmented virtual and physical mappings.
- **1 GB mode**: One large virtual region → virtual continuity, but physical frames remain scattered.

The 1 GB block required explicitly touching each page to trigger real allocation (lazy paging).

#### **Additional insights**
- Large blocks appear unmapped until touched — confirming demand paging.
- Thousands of small allocations in 4 KB mode slow down the program due to fragmentation and metadata overhead.

---

### 🔹 Kernel-Space Observations

#### **kmalloc vs vmalloc (continuity)**
- **kmalloc**: Both VA and PA are mostly continuous, because memory comes from the **physically contiguous direct-mapped region**.
- **vmalloc**: VA is continuous, but PA is **highly non-contiguous**, showing large jumps between pages.

#### **Is the offset related to allocation size?**
No.  
`kmalloc` uses a linear direct map, so offsets appear consistent.  
`vmalloc` uses dispersed pages, so no offset pattern exists.

#### **Differences between kmalloc and vmalloc**
- `kmalloc`: Fast, cache-friendly, but limited by contiguous physical memory.
- `vmalloc`: More flexible, supports large allocations, but incurs page-table overhead and slower access.

#### **User-space vs Kernel-space allocations**
User-space allocations (`malloc`, `calloc`) hide page-table mechanics behind abstractions.  
Kernel allocations explicitly show page mapping behavior and give developers control over contiguity.

#### **Other interesting behaviors**
- `vmalloc` requires touching pages (via `memset`) to ensure they are mapped.
- `kmalloc` does not require touching — memory is allocated eagerly.
- Large `kmalloc` allocations fail easily due to physical memory fragmentation.

---

### 🔹 1GB Kernel Allocation

- **vmalloc**:  
  Typically succeeds. Pages are scattered across physical memory but virtually mapped contiguously.
  
- **kmalloc**:  
  Usually fails. The kernel cannot find a 1 GB **physically contiguous** region because of fragmentation.

This demonstrates why the kernel provides both allocators:  
`kmalloc` for small, fast, contiguous allocations — `vmalloc` for large or fragmented ones.

---

