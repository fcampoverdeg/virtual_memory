/*
 *  memalloc.c - CS5204 Task2 userspace page-table walker
 *  -----------------------------------------------------
 *
 *  This program allocates memory in user space in two different ways and uses /proc/self/pagemap (via
 *  pagemap.c/pagemap.h) to translate each page's Virtual Address (VA) to its Physical Address (PA)
 *
 *  Builds two allocation modes:
 *  	-b 4K : allocate N 4KB blocks via double-pointer array
 *  	-b 1G : allocate one big block (size controlled by -m), then touch per page
 *
 *  Options:
 *  	-m <size> : Total size defaults to 1G, override with -m (Example: -m 512M, -m 2G, -m 500MB).
 *  	-o <file> : output file (default "out")
 *  	
 *  Output format per line: VA=0x%lx -> PA=0x%lx
 *
 *  Notes:
 *  	- I explicitly "touch" pages to ensure demand-paged memroy is backed by a frame.
 *  	- Used uintptr_t/uint64_t to avoid overflow on 64-bit systems.
 */


#include <stdio.h> // for fprintf
#include <stdlib.h>  // for malloc
#include <unistd.h> // for optarg

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pagemap.h"	// Exposes pagemap_va2pa(), declared by the starter


// Page constants (assumes 4KiB pages). (**Adjust if the VM differs**)
#define	PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)

/* =======================================================================
 * Utility: simple fatal error printer
 * ======================================================================= */

static void die(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}


/* =======================================================================
 * Utility: Parse size strings like 1G, 512M, 4K into bytes
 * ======================================================================= */

static uint64_t parse_size_str(const char *s) {
	/*
	 * Accept forms like: 4096, 4K, 4k, 512M, 1G, 500MB, 2GB (case insensitive (more utility))
	 * Units: K, M, G with optional "B" suffix
	 * if no unit provided, assume it is in bytes.
	 */
	if (s == NULL || *s == '\0') {
		fprintf(stderr, "Invalid size string\n");
		exit(EXIT_FAILURE);
	}

	// Copy and normalize
	char buf[64];
	size_t n = strlen(s);

	if (n >= sizeof(buf)) {
		fprintf(stderr, "Size string too long\n");
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < n; i++) buf[i] = tolower((unsigned char)s[i]);
	buf[n] = '\0';

	// Find first non-digit / period
	size_t i = 0;
	while (i < n && (isdigit((unsigned char)buf[i]) || buf[i] == '.')) i++;

	double value = 0.0;
	if (i == 0) {
		fprintf(stderr, "Invalid numeric prefix in size: %s\n", s);
		exit(EXIT_FAILURE);
	}

	// parse the numeric part (accepts integer-like; decimals will be truncated)
	
	{
		char numpart[64];
		memcpy(numpart, buf, i);
		numpart[i] = '\0';
		value = atof(numpart);
	}


	// Calculate the size needed depending on input from prompt
	uint64_t mult = 1;
	const char *unit = buf + i;
	
	if (*unit == '\0') {
		mult = 1; // bytes
	} else if (strcmp(unit, "k") == 0 || strcmp(unit, "kb") == 0) {
		mult = 1024ULL;
	} else if (strcmp(unit, "m") == 0 || strcmp(unit, "mb") == 0) {
		mult = 1024ULL * 1024ULL;
	} else if (strcmp(unit, "g") == 0 || strcmp(unit, "gb") == 0) {
		mult = 1024ULL * 1024ULL * 1024ULL;
	} else {
		fprintf(stderr, "Unknown size unit: '%s'\n", unit);
		exit(EXIT_FAILURE);
	}

	// Truncate to integer bytes
	long double bytes = (long double)value * (long double)mult;
	if (bytes < 0.0L) {
		fprintf(stderr, "Negative size not allowed\n");
		exit(EXIT_FAILURE);
	}

	if (bytes > (long double)UINT64_MAX) {
		fprintf(stderr, "Size too large\n");
		exit(EXIT_FAILURE);
	}
	return (uint64_t)bytes;
}


/* =======================================================================
 * Utility: Open /proc/self/pagemap
 * ======================================================================= */

static int open_pagemap_self(void) {
	int fd = open("/proc/self/pagemap", O_RDONLY);
	if (fd < 0) die("open(/proc/self/pagemap)");
	return fd;
}

/* =======================================================================
 * Utility: Write VA -> PA mapping line to output file
 * ======================================================================= */
static void write_va_pa(FILE *f, void *va, uint64_t pa) {
	// Lines like: VA=0x295952a0 -> PA=0x25f89e2a0
	// Cast VA to uintptr_t to print as 0x%lx portably on LP64.
	fprintf(f, "VA=0x%lx -> PA=0x%lx\n",
			(unsigned long)(uintptr_t)va,
			(unsigned long)pa);
}


/* -----------------------------------------------------------------------
 * Case 1: 4KB allocation mode (many small pages)
 * -----------------------------------------------------------------------
 *
 *  - Allocates N individual 4KB blocks (double pointer array)
 *  - Touches each to fault it in
 *  - Log VA -> PA per block
 *
 *
 *  ** May crash when running it, depends on your system's settings **
 */

static void mode_blocks_4k(uint64_t total_bytes, const char *outfile) {
	FILE *f = fopen(outfile, "w");
	if (!f) die("fopen(output)");
	int pmfd = open_pagemap_self();

	// Number of 4K pages to allocate
	if (total_bytes == 0) {
		fprintf(stderr, "Total size must be > 0 \n");
		fclose(f);
		close(pmfd);
		exit(EXIT_FAILURE);
	}
	uint64_t npages = (total_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

	// Allocate array of pointers
	void **blocks = (void**)calloc(npages, sizeof(void*));
	if (!blocks) {
		fclose(f);
		close(pmfd);
		die("calloc(pointer array)");
	}

	// Allocate each 4K block (zero-initialized). Touch is implied by calloc, but I will write 1 byte to be specific.
	for (uint64_t i = 0; i < npages; i++) {
		blocks[i] = calloc(i, PAGE_SIZE);
		if (!blocks[i]) {
			fprintf(stderr, "calloc failed at page %" PRIu64 "\n", i);

			// free already allocated memory and abort
			free(blocks);
			fclose(f);
			close(pmfd);
			exit(EXIT_FAILURE);
		}

		// Ensuring the page is faulted
		((volatile char*)blocks[i])[0] = 1;

		uint64_t pa = pagemap_va2pa(pmfd, blocks[i]);
		write_va_pa(f, blocks[i], pa);
	}

	// Cleanup
	for (uint64_t i = 0; i < npages; i++) free(blocks[i]);
	free(blocks);
	fclose(f);
	close(pmfd);
}

/* -----------------------------------------------------------------------
 * Case 2: 1GB (single large block) allocation mode
 * -----------------------------------------------------------------------
 *
 *  - Allocates one large, contiguous virtual region
 *  - Touches each 4KB page (lazy allocation)
 *  - Logs VA -> VA per range
 */

static void mode_big_block(uint64_t total_bytes, const char *outfile) {
	if (total_bytes == 0) {
		fprintf(stderr, "Total size must be > 0\n");
		exit(EXIT_FAILURE);
	}

	// Round up to page size to simplify stepping
	uint64_t rounded = (total_bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	FILE *f = fopen(outfile, "w");
	if (!f) die("fopen(output)");
	int pmfd = open_pagemap_self();

	void *base = calloc(1, rounded);
	if (!base) {
		fclose(f);
		close(pmfd);
		die("calloc(1G block)");
	}

	// Touch each page to ensure mapping is established (malloc behaves lazy)
	for (uint64_t off = 0; off < rounded; off+= PAGE_SIZE) {
		((volatile char*)base)[off] = 1;
	}

	// Walking per page
	for (uint64_t off = 0; off < rounded; off += PAGE_SIZE) {
		void *va = (void*)((uintptr_t)base + off);
		uint64_t pa = pagemap_va2pa(pmfd, va);
		write_va_pa(f, va, pa);
	}

	free(base);
	fclose(f);
	close(pmfd);
}
		
/* =======================================================================
 * Main: Argument parsing and mode dispatch
 * ======================================================================= */
int main(int argc, char **argv) {
	
	const char *block_mode 	= "4K";		// -b {4K|1G}
	const char *outfile 	= "out";	// -o <file>
	const char *totalsz 	= "1G";		// -m <size>, default 1G total

	int c;

	// Use "b:m:o:"
	while ((c = getopt(argc, argv, "b:m:o:")) != -1) {
		switch (c) {
			case 'b':
				block_mode = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'm':
				totalsz = optarg;
				break;
			default:
				fprintf(stderr,
					"Usage: %s [-b {4K|1G}] [-o <output>]\n"
					"   -b : allocation mode; '4K' = array of 4KB blocks, '1G' = one big block\n"
					"   -m : total size to allocate (default 1G). e.g., 512 M, 1G, 500MB\n"
					"   -o : output file for lines 'VA=0x... -> PA=0x...'\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}


	// Normalizing -b (case-insensitive)
	char bm[8];
	size_t blen = strlen(block_mode);

	if (blen >= sizeof(bm)) {
		fprintf(stderr, "Invalid -b argument\n");
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < blen; i++) bm[i] = tolower((unsigned char) block_mode[i]);
	bm[blen] = '\0';

	uint64_t total_bytes = parse_size_str(totalsz);

	if (strcmp(bm, "4k") == 0) {
		mode_blocks_4k(total_bytes, outfile);
	} else if (strcmp(bm, "1g") == 0) {
		// In "1G" it is possible to use -m (total) still, it is to give it more flexibility
		// with using -b 1G -m 256M in case the VM has less memory
		mode_big_block(total_bytes, outfile);
	} else {
		fprintf(stderr, "Unknown -b mode '%s' (use 4K or 1G)\n", block_mode);
		exit(EXIT_FAILURE);
	}

	return 0;
}
