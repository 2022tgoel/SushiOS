// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13

struct {
  struct spinlock lock;
  struct spinlock block_locks[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.block_locks[i], "bcache.bucket");
  }

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  int i = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->ind = i++;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // acquire(&bcache.lock);
  // lock needed for refcount 
  // race condition: 
  // process claims block
  // before it can set refcount to one, another process claims block
  // also dev/blockno updates can cause race conditions
  // don't want one process to update a buf and other process to not find it 
  acquire(&bcache.block_locks[blockno % NBUCKETS]);
  // printf("%d\n", blockno % NBUCKETS);
  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.block_locks[blockno % NBUCKETS]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.block_locks[blockno % NBUCKETS]);
  // for (int i = 0; i < NBUCKETS; i++) {
  //   // acquire all the locks.
  //   // This prevents someone from two processes from claiming
  //   // a refcount=0 block for themselves
  //   acquire(&bcache.block_locks[i]);
  // }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev) {
    int no = b->blockno % NBUCKETS;
    acquire(&bcache.block_locks[no]);
    acquire(&bcache.lock);
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      // for (int i = 0; i < NBUCKETS; i++) {
      //   release(&bcache.block_locks[i]);
      // }
      release(&bcache.block_locks[no]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bcache.block_locks[no]);
    release(&bcache.lock);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.block_locks[b->blockno % NBUCKETS]);
  acquire(&bcache.lock);
  // printf("%d\n", b->blockno % NBUCKETS);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
    
  }
  release(&bcache.lock);
  release(&bcache.block_locks[b->blockno % NBUCKETS]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.block_locks[b->blockno % NBUCKETS]);
  b->refcnt++;
  release(&bcache.block_locks[b->blockno % NBUCKETS]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.block_locks[b->blockno % NBUCKETS]);
  b->refcnt--;
  release(&bcache.block_locks[b->blockno % NBUCKETS]);
}


