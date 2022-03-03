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
#include "limits.h"
// lab8: task buffer cache
// using hashing to increase throughput
// Design:
// implement 13 hash buckets, ensuring that block hashed to the same number would be found on the same bucket.
// the problem arises when redesigning beget.
// 1. when a buffer cache miss is encountered, before find LRU buffer in other bucket, we have to unlock the current bucket, unless a deadlock would happen.
// 2. after releasing the lock, before appending the previous bucket with LRU buffer cache, the previous buffer could be already appended with the same block, which breaks the invariant that only one buffe cache for each block.
// sulution to 2: after releasing the lock, grab a global LRU seeking lock.
// only thread taking such a lock could find lRU cache and append it.
// thus saving the invariant just mentioned.

// step1. init these buckets.
// step2. redesing buf: add timestamp field.
// step3. redesign bget
// step4. redesign berelse.

struct {
  struct spinlock LRUseek;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf heads[13];
  struct spinlock bucket[13];

} bcache;

void
binit(void)
{

  initlock(&bcache.LRUseek, "bcache-LRUseek");
   
  for(int i = 0; i < 13; i++) {
    bcache.heads[i].prev = &bcache.heads[i];
    bcache.heads[i].next = &bcache.heads[i];
    initlock(&bcache.bucket[i], "bcache-bucket");
  }

  for(int i = 0; i < NBUF; i++) {
    initsleeplock(&bcache.buf[i].lock, "buf");
    int t = i%13;  // add the current buf in bucket t;
    bcache.buf[i].prev = bcache.heads[t].prev;
    bcache.buf[i].next = &bcache.heads[t];
    bcache.heads[t].prev->next = &bcache.buf[i];
    bcache.heads[t].prev = &bcache.buf[i];
    bcache.buf[i].timestamp = ticks;
    
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{

// hash to a certain bucket.
  int t = (dev + blockno)%13;
  acquire(&bcache.bucket[t]);

// search in the current bucket
  struct buf* cur_buf = bcache.heads[t].next;

  do {
    if(cur_buf->dev == dev && cur_buf->blockno == blockno) {
      cur_buf->refcnt++;
      release(&bcache.bucket[t]);
      acquiresleep(&cur_buf->lock);
      return cur_buf;	  
    }else {
      cur_buf = cur_buf->next;
    }

  }while(cur_buf != &bcache.heads[t]);

// matching cache not found, try to seek in other bucket;
// search in the previous bucket again, since it may have changed
// during our acquisition of the global spinclock LRUseek  
  release(&bcache.bucket[t]);
  acquire(&bcache.LRUseek);
  acquire(&bcache.bucket[t]);
  cur_buf = bcache.heads[t].next;
  
  do {
    if(cur_buf->dev == dev && cur_buf->blockno == blockno) {
      cur_buf->refcnt++;
      release(&bcache.bucket[t]);
      release(&bcache.LRUseek);
      acquiresleep(&cur_buf->lock);
      return cur_buf;
    }else {
      cur_buf = cur_buf->next;
    }
  }while(cur_buf != &bcache.heads[t]);

// still not found in the previous bucket, envict the LRU buffer cache among all buffer cache.
  release(&bcache.bucket[t]);

// invariance: during the whole process of seeking LRU and restructuring bucket,the LRU seek global lock is hold.
  uint min = INT_MAX;
  uint prevLRUbucket = 14;// if found, it should be < 13
  struct buf* cur;
  struct buf* saved = &bcache.heads[t];

  for(int i = 0; i < 13; i++) {

    cur = bcache.heads[i].next;
    acquire(&bcache.bucket[i]); // avoid reaquisition of same lock // changed.

    do{
      if(cur->refcnt == 0 && cur->timestamp < min) {
        min = cur->timestamp;
        if(prevLRUbucket != i && prevLRUbucket < 14) {
	  release(&bcache.bucket[prevLRUbucket]);
	}
        prevLRUbucket = i;
	saved = cur;
      }
      cur = cur->next;
    }while(cur != &bcache.heads[i]);

    //invariance: the lock on the bucket where the currently LRU cache is found is kept.
    if(prevLRUbucket != i) { // if LRU found in current buck, do not release.
      release(&bcache.bucket[i]);
    }

  }

// whether a free buffer cache has been found
  if(prevLRUbucket == 14) {
    panic("not enough free cache");
  }

// steal a buffer from other bucket; 

// off the original bucket.
   saved->next->prev = saved->prev;
   saved->prev->next = saved->next;
   release(&bcache.bucket[prevLRUbucket]);

// on the end of new bucket;
// remember to acquire the lock
   acquire(&bcache.bucket[t]);
   bcache.heads[t].prev->next = saved;
   saved->prev = bcache.heads[t].prev;   
   bcache.heads[t].prev = saved;
   saved->next = &bcache.heads[t];
  
   saved->dev = dev;
   saved->blockno = blockno;
   saved->refcnt = 1;
   saved->valid = 0;
   saved->timestamp = ticks;
   release(&bcache.bucket[t]); 
  release(&bcache.LRUseek);
   acquiresleep(&saved->lock);
   return saved;
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

  acquire(&bcache.bucket[(b->dev + b->blockno)%13]);
  b->refcnt--;
  if(b->refcnt == 0)  b->timestamp = ticks;
  release(&bcache.bucket[(b->dev + b->blockno)%13]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.bucket[(b->dev + b->blockno)%13]);
  b->refcnt++;
  release(&bcache.bucket[(b->dev + b->blockno)%13]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.bucket[(b->dev + b->blockno)%13]);
  b->refcnt--;
  release(&bcache.bucket[(b->dev + b->blockno)%13]);
}


