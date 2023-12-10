#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "defs.h"
#include "memlayout.h"
#include "mmap.h"


// Copy the mmap regions from src to dest
// First arg: dest
// Second arg: src
void mmp_cpy(struct mmp *mr1, struct mmp *mr2) {
  mr1->addr = mr2->addr;
  mr1->length = mr2->length;
  mr1->flags = mr2->flags;
  mr1->prot = mr2->prot;
  mr1->f = mr2->f;
  mr1->offset = mr2->offset;
}

// Get physical Address of page from virtual address of process
uint get_physical_page(struct proc *p, uint tempaddr, pte_t **pte) {
  *pte = walkpgdir(p->pgdir, (char *)tempaddr, 0);
  if (!*pte) {
    return 0;
  }
  uint pa = PTE_ADDR(**pte);
  return pa;
}

/// Helper function to copy a single mmap entry from parent to child.
static int copy_single_mmap(struct proc *parent, struct proc *child, int index) {
  pte_t *pte;
  uint virt_addr = parent->mmaps[index].addr;
  int protection = parent->mmaps[index].prot;
  int is_shared = parent->mmaps[index].flags & MAP_SHARED;
  uint length = parent->mmaps[index].length;
  uint offset = parent->mmaps[index].offset;

  for (uint start = virt_addr; start < virt_addr + length; start += PGSIZE) {
    uint pa = get_physical_page(parent, start, &pte);
    if (is_shared && pa == 0) {  // Allocate and store data if shared and not yet done.
      int remaining_length = parent->mmaps[index].length - parent->mmaps[index].stored_length;
      int chunk_length = (PGSIZE > remaining_length) ? remaining_length : PGSIZE;
      if (mmap_store_data(parent, start, chunk_length, parent->mmaps[index].flags, protection, parent->mmaps[index].f, offset) < 0) {
        return -1;
      }
      parent->mmaps[index].stored_length += chunk_length;
      pa = get_physical_page(parent, start, &pte);
    }
    if (pa == 0 && !is_shared) {  // Skip private pages not yet allocated.
      continue;
    }
    char *mem = (is_shared) ? (char *)P2V(pa) : kalloc();
    if (!mem) {
      return -1; // ERROR: kalloc failed for private page or P2V failed for shared page.
    }
    if (!is_shared) {
      memmove(mem, (char *)P2V(pa), PGSIZE); // Copy data for private mapping.
    }
    if (mappages(child->pgdir, (void *)start, PGSIZE, V2P(mem), protection) < 0) {
      if (!is_shared) {
        kfree(mem); // Free allocated memory on failure if private.
      }
      return -1; // ERROR: mappages failed.
    }
  }

  // Copy mmap metadata from parent to child.
  mmp_cpy(&child->mmaps[index], &parent->mmaps[index]);
  if (is_shared) {
    child->mmaps[index].ref = 1;
  }

  return 0; // Success.
}

// Copy mmaps from parent to child process.
int copy_maps(struct proc *parent, struct proc *child) {
  for (int i = 0; i < parent->total_mmaps; i++) {
    if (copy_single_mmap(parent, child, i) < 0) {
      return -1; // Propagate failure from copying a single mmap.
    }
  }

  child->total_mmaps = parent->total_mmaps;
  return 0; // Success.
}


// Right shift the array and add the mappings at i + 1 index
int mmp_insert(struct proc *p, int length, int i, uint mmapaddr) {
  int j = p->total_mmaps;
  while (j > i + 1) {
    mmp_cpy(&p->mmaps[j], &p->mmaps[j - 1]);
    j--;
  }
  if (PGROUNDUP(mmapaddr + length) >= KERNBASE) {
    // Address Exceeds KERNBASE
    return -1;
  }
  p->mmaps[i + 1].addr = mmapaddr;
  p->mmaps[i + 1].length = length;
  return i + 1; // Return the index of mmap mapping
}

// To check if mmap is possible at user-provided address
int mmp_check(struct proc *p, uint addr, int length) {
  uint rounded_addr = addr; // Assume addr is already rounded up to page boundary
  int last_index = p->total_mmaps - 1;

  // Check if there's space after the last mmap entry
  if (rounded_addr > PGROUNDUP(p->mmaps[last_index].addr + p->mmaps[last_index].length)) {
    return mmp_insert(p, length, last_index, rounded_addr);
  }

  // Iterate through the mmap entries to find a gap large enough for the requested mapping
  int i = 0;
  while (i < last_index) {
    uint next_mmap_start = PGROUNDUP(p->mmaps[i + 1].addr);
    uint current_mmap_end = PGROUNDUP(p->mmaps[i].addr + p->mmaps[i].length);

    // Check if there's a gap between the current and next mmap entry
    if (rounded_addr >= current_mmap_end && next_mmap_start >= rounded_addr + length) {
      return mmp_insert(p, length, i, rounded_addr);
    }
    
    // Move to the next mmap entry
    i++;
  }

  // If no space is found, return error
  return -1;
}


// To find the mmap region virtual address
int mmp_any(struct proc *p, int length) {
  if (p->total_mmaps == 0) {
    if (PGROUNDUP(MMAPBASE + length) >= KERNBASE) {
      // Address Exceeds KERNBASE
      return -1;
    }
    p->mmaps[0].addr = PGROUNDUP(MMAPBASE);
    p->mmaps[0].length = length;
    return 0; // Return the index in mmap region array
  }
  int i = 0;
  uint mmapaddr;
  // If mapping is possible between MMAPBASE & first mapping
  if (p->mmaps[0].addr - MMAPBASE > length) {
    mmapaddr = MMAPBASE;
    return mmp_insert(p, length, -1, mmapaddr);
  }
  // Find the map address
  while (i < p->total_mmaps && p->mmaps[i + 1].addr != 0) {

    uint start_addr = PGROUNDUP(p->mmaps[i].addr + p->mmaps[i].length);
  
    uint end_addr = PGROUNDUP(p->mmaps[i + 1].addr);
    if (end_addr - start_addr > length + (p->mmaps[i].flags & MAP_GROWSUP ? PGSIZE : 0)) {
      
      break;
    }
    i += 1;
  }
  mmapaddr = PGROUNDUP(p->mmaps[i].addr + p->mmaps[i].length);
  if (mmapaddr + length > KERNBASE) {
    return -1;
  }
  // Right shift the mappings to arrange in increasing order
  return mmp_insert(p, length, i, mmapaddr);
}

static int map_pagecache_page(struct proc *p, struct file *f, uint mmapaddr,
                              int protection, int offset, int length) {
  for (int currlength = 0; currlength < length; currlength += PGSIZE) {
    char *temp = kalloc(); // Allocate a temporary page
    if (!temp) {
      // Kalloc failed
      return -1;
    }
    memset(temp, 0, PGSIZE);

    // copy the file content from page cache to allocated memory
    int toRead = PGSIZE;
    if (length - currlength < PGSIZE) {
      toRead = length - currlength;
    }

    int currOffset = offset + currlength;
    for (int i = 0; toRead > 0; i++) {
      int pageOffset = currOffset % PGSIZE;
      int readLength = PGSIZE - pageOffset < toRead ? PGSIZE - pageOffset : toRead;
      if (pageOffset > f->ip->size) {
        break;
      }
      if (copyPage(f->ip, currOffset, f->ip->inum, f->ip->dev, temp + PGSIZE * i,
                   readLength, pageOffset) == -1) {
        kfree(temp);
        return -1;
      }
      toRead -= readLength;
      currOffset += readLength;
    }

    // Map the page to user process
    if (mappages(p->pgdir, (void *)(mmapaddr + currlength), PGSIZE, V2P(temp), protection) < 0) {
      kfree(temp);
      return -1;
    }
  }
  return 0;
}

static int map_anon_main(struct proc *p, uint mmapaddr, int protection,
                         int length) {
  for (int i = 0; i < length; i += PGSIZE) {
    char *mapped_page = kalloc();
    if (!mapped_page) {
      // Kalloc failed
      return -1;
    }
    memset(mapped_page, 0, PGSIZE);
    
    if (mappages(p->pgdir, (void *)(mmapaddr + i), PGSIZE, V2P(mapped_page), protection) < 0) {
      // mappages failed
      deallocuvm(p->pgdir, mmapaddr + i - PGSIZE, mmapaddr + i);
      kfree(mapped_page);
      return -1;
    }
  }
  return 0;
}

int mmap_store_data(struct proc *p, int addr, int length, int flags,
                    int protection, struct file *f, int offset) {
  if (!(flags & MAP_ANONYMOUS)) { // File backed mapping
    if (map_pagecache_page(p, f, addr, protection, offset, length) < 0) {
      return -1;
    }
  } else { // Anonymous mapping
    if (map_anon_main(p, addr, protection, length) < 0) {
      return -1;
    }
  }
  return 0;
}


// mmap system call main function
void *my_mmap(int addr, struct file *f, int length, int offset, int flags, int protection) {
    // Check for invalid arguments
    if ((!(flags & MAP_PRIVATE) && !(flags & MAP_SHARED)) ||
        length <= 0 || offset < 0 ||
        (!(flags & MAP_ANONYMOUS) && !f->readable) ||
        ((flags & MAP_SHARED) && (protection & PROT_WRITE) && !f->writable)) {
        return (void *)-1;
    }

    struct proc *p = myproc();
    // Limit of mappings reached
    if (p->total_mmaps == 32) {
        return (void *)-1;
    }

    // If addr is provided, it should be page-aligned and within bounds.
    uint rounded_addr = PGROUNDUP(PGROUNDUP(addr) + length);
    if (addr && (addr < MMAPBASE || rounded_addr > KERNBASE || addr % PGSIZE != 0)) {
        return (void *)-1;
    }

    int i = -1;
    if (flags & MAP_FIXED) {
        // When MAP_FIXED is specified, check if the address is possible
        i = addr ? mmp_check(p, (uint)addr, length) : -1;
    } else {
        // For non-fixed mapping, find a suitable address
        i = addr ? mmp_check(p, (uint)addr, length) : mmp_any(p, length);
    }

    if (i == -1) {
        return (void *)-1;
    }

    // Store mmap information in the process's mmap array
    p->mmaps[i].flags = flags;
    p->mmaps[i].prot = (protection == PROT_NONE) ? 0 : (PTE_U | protection);
    p->mmaps[i].offset = offset;
    p->mmaps[i].f = f;
    p->mmaps[i].addr = (flags & MAP_FIXED) ? addr : (int)p->mmaps[i].addr;
    p->total_mmaps++;

    return (void *)p->mmaps[i].addr;
}


int my_munmap(struct proc *p, int addr, int length) {
  pte_t *pte;
  uint mainaddr = PGROUNDUP(addr);
  int unmapping_length = PGROUNDUP(length);
  int i = 0;
  int total_length = 0;
  for (; i < 32; i++) {
    if (p->mmaps[i].addr == mainaddr) {
      total_length = p->mmaps[i].length;
      break;
    }
  }
  if (i == 32 || total_length == 0) {
    return -1;
  }
  if ((p->mmaps[i].flags & MAP_SHARED) && !(p->mmaps[i].flags & MAP_ANONYMOUS) && (p->mmaps[i].prot & PROT_WRITE)) {
    p->mmaps[i].f->off = p->mmaps[i].offset;
    if (filewrite(p->mmaps[i].f, (char *)p->mmaps[i].addr, p->mmaps[i].length) < 0) {
      return -1;
    }
  }

  int currlength = 0;
  int main_map_length = unmapping_length > total_length ? total_length: unmapping_length;
  for (; currlength < main_map_length; currlength += PGSIZE) {
    uint pa = get_physical_page(p, addr + currlength, &pte);
    if (pa == 0) {
      continue;
    }
    char *v = P2V(pa);
    kfree(v);
    *pte = 0;
  }
  if (p->mmaps[i].length <= unmapping_length) {
    p->mmaps[i].addr = 0;
    p->mmaps[i].length = 0;
    p->mmaps[i].flags = 0;
    p->mmaps[i].prot = 0;
    p->mmaps[i].f = 0;
    p->mmaps[i].offset = 0;
    p->mmaps[i].flags = 0;
    p->mmaps[i].stored_length = 0;

    while (i < 32 && p->mmaps[i + 1].addr) {
      mmp_cpy(&p->mmaps[i], &p->mmaps[i + 1]);
      i += 1;
    }
    p->total_mmaps -= 1;
  } else {
    p->mmaps[i].addr += unmapping_length;
    p->mmaps[i].length -= unmapping_length;
  }
  return 0;
}