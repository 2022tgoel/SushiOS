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
void freerange_hart(void *pa_start, void *pa_end, int hart);
void kfree_hart(void *pa, int hart);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct mem {
  struct spinlock lock;
  struct run *freelist;
};

struct mem kmem[NCPU];

void
kinit()
{
  char name[7];
  uint64 memsz = (PHYSTOP - (uint64) end) / NCPU;
  for (int hart = 0; hart < NCPU; hart++){
    memset(name, 0, 7);
    snprintf(name, 7, "kmem %d", hart);
    initlock(&kmem[hart].lock, name);

    uint64 startaddr = ((uint64) end + hart*memsz);
    printf("creating freelist starting from: %p and going to: %p\n", startaddr, (startaddr + memsz));
    freerange_hart((void*)startaddr, (void*)(startaddr + memsz), hart);
  }
  
}

void 
freerange_hart(void *pa_start, void *pa_end, int hart){
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_hart(p, hart);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

void
kfree_hart(void *pa, int hart)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[hart].lock);
  r->next = kmem[hart].freelist;
  kmem[hart].freelist = r;
  release(&kmem[hart].lock);
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int hart = cpuid();
  pop_off();

  acquire(&kmem[hart].lock);
  r->next = kmem[hart].freelist;
  kmem[hart].freelist = r;
  release(&kmem[hart].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int hart = cpuid();
  pop_off();

  for (int i = 0; i < NCPU; i++){
    int j = (i + hart) % NCPU;
    acquire(&kmem[j].lock);
    r = kmem[j].freelist;
    if(r)
      kmem[j].freelist = r->next;
    release(&kmem[j].lock);
    if(r) 
      break;
  }
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  else {
    printf("could not allocate page\n");
    // for (int i = 0; i < NCPU; i++){
    //   int j = (i + hart) % NCPU;
    //   printf("trying freelist %d\n", j);
    //   acquire(&kmem[j].lock);
    //   r = kmem[j].freelist;
    //   printf("address of page: %p\n", r);
    //   if(r)
    //     kmem[j].freelist = r->next;
    //   release(&kmem[j].lock);
    //   if(r)
    //     break;
    // }
  }
  // printf("giving cpu %d physical mem %p\n", hart, r);
  return (void*)r;
}
