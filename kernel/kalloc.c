// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

uint8 pageref[NUMPAGES]; // number of references to each page

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // printf("resetting the reference counts\n");
  memset((char *) pageref, 0, NUMPAGES);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  printf("freerange %p\n", r_sp());
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  printf("%p\n", *((uint64 *) r_sp()));
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  int pagenum = PA2PAGE(pa);
  if (pageref[pagenum]) {
    pageref[pagenum]--;
    // printf("decrementing the reference count %d for page %d\n ", pageref[pagenum], pagenum);
  }

  if (pageref[PA2PAGE(pa)] >= 1) {
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    pageref[PA2PAGE(r)]++;
    // printf("incrementing the reference count %p, page num %d\n", r, PA2PAGE(r));
  }
  return (void*)r;
}
