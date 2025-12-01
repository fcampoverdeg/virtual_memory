#ifndef PAGEMAP_H
#define PAGEMAP_H

#include <stdint.h>

uint32_t page_offset(uint32_t addr);
uint64_t pagemap_va2pfn(int pagemapfd, void* addr);
uint64_t pagemap_va2pa(int pagemapfd, void* addr);

#endif // PAGEMAP_H
