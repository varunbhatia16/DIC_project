
#ifndef __L2_ATOMIC_QUEUE__
#define __L2_ATOMIC_QUEUE__

#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include "spi/include/l2/atomic.h"
#include "spi/include/l1p/flush.h"
#include "pcqueue.h"

#define DEFAULT_SIZE         1024
#define L2_ATOMIC_FULL        0x8000000000000000UL
#define L2_ATOMIC_EMPTY       0x8000000000000000UL

#define L2A_SUCCESS  0
#define L2A_EAGAIN  -1
#define L2A_FAIL    -2

#define __L2_ATOMIC_QUEUE_BLOCKING  1

typedef  void* L2AtomicQueueElement;

typedef struct _l2atomicstate {
  volatile uint64_t Consumer;	// not used atomically
  volatile uint64_t Producer;
  volatile uint64_t UpperBound;
  volatile uint64_t Flush;	// contents not used
} L2AtomicState;

typedef struct _l2atomicq {
  L2AtomicState               * _l2state;
  volatile void * volatile    * _array;
  int                           _useOverflowQ;
  int                           _qsize;
  PCQueue                       _overflowQ;
  pthread_mutex_t               _overflowMutex;
} L2AtomicQueue;

void L2AtomicQueueInit      (void           * l2mem, 
			     size_t           l2memsize, 
			     L2AtomicQueue  * queue,
			     int              use_overflow,
			     int              nelem) 
{
  pami_result_t rc;
  
  //Verify counter array is 64-byte aligned 
  assert( (((uintptr_t) l2mem) & (0x1F)) == 0 );  
  assert (sizeof(L2AtomicState) <= l2memsize);
  
  queue->_useOverflowQ = use_overflow;

  int qsize = 2;
  while (qsize < nelem) 
    qsize *= 2;
  queue->_qsize = qsize;

  queue->_l2state = (L2AtomicState *)l2mem;
  pthread_mutex_init(&queue->_overflowMutex, NULL);
  queue->_overflowQ = PCQueueCreate();
  L2_AtomicStore(&queue->_l2state->Consumer, 0);
  L2_AtomicStore(&queue->_l2state->Producer, 0);
  L2_AtomicStore(&queue->_l2state->UpperBound, qsize);
  
  rc = posix_memalign ((void **)&queue->_array,
		       64, /*L1 line size for BG/Q */
		       sizeof(L2AtomicQueueElement) * qsize);

  assert(rc == PAMI_SUCCESS);
  memset((void*)queue->_array, 0, sizeof(L2AtomicQueueElement)*qsize);
}

int L2AtomicEnqueue (L2AtomicQueue          * queue,
		     void                   * element) 
{
  //fprintf(stderr,"Insert message %p\n", element);

  register int qsize_1 = queue->_qsize - 1;
  uint64_t index = L2_AtomicLoadIncrementBounded(&queue->_l2state->Producer);
  L1P_FlushRequests();
  if (index != L2_ATOMIC_FULL) {
    queue->_array[index & qsize_1] = element;
    return L2A_SUCCESS;
  }
  
  //We dont want to use the overflow queue
  if (!queue->_useOverflowQ)
    return L2A_EAGAIN; //Q is full, try later
  
  //No ordering is guaranteed if there is overflow
  pthread_mutex_lock(&queue->_overflowMutex);
  PCQueuePush(queue->_overflowQ, element);
  pthread_mutex_unlock(&queue->_overflowMutex);
  
  return L2A_SUCCESS;
}

void * L2AtomicDequeue (L2AtomicQueue    *queue)
{
  uint64_t head, tail;
  tail = queue->_l2state->Producer;
  head = queue->_l2state->Consumer;
  register int qsize_1 = queue->_qsize-1;

  volatile void *e = NULL;
  if (head < tail) {    
    e = queue->_array[head & qsize_1];
#if __L2_ATOMIC_QUEUE_BLOCKING
    while (e == NULL) 
      e = queue->_array[head & qsize_1];
#else
    if (e == NULL)
      return NULL;
#endif

    queue->_array[head & qsize_1] = NULL;
    ppc_msync();

    head ++;
    queue->_l2state->Consumer = head;    
    
    //Charm++ does not require message ordering
    //So we dont acquire overflow mutex here
    uint64_t n = head + queue->_qsize;
    // is atomic-store needed?
    L2_AtomicStore(&queue->_l2state->UpperBound, n);
    return (void*) e;
  }

  //We dont have an overflowQ
  if (!queue->_useOverflowQ)
    return NULL;
  
  /* head == tail (head cannot be greater than tail) */
  if (PCQueueLength(queue->_overflowQ) > 0) {
    pthread_mutex_lock(&queue->_overflowMutex);      
    e = PCQueuePop (queue->_overflowQ);    
    pthread_mutex_unlock(&queue->_overflowMutex);      
    
    return (void *) e;
  }

  return (void *) e;
}

int L2AtomicQueueEmpty (L2AtomicQueue *queue) {
  return ( (PCQueueLength(queue->_overflowQ) == 0) &&
	   (queue->_l2state->Producer == queue->_l2state->Consumer) );
}

//spin block in the L2 atomic queue till there is a message. fail and
//return after n iterations
int L2AtomicQueueSpinWait (L2AtomicQueue    * queue,
			   int                n)
{
  if (!L2AtomicQueueEmpty(queue))
    return 0;  //queue is not empty so return
  
  uint64_t head, tail;
  head = queue->_l2state->Consumer;
  
  size_t i = n;
  do {
    tail = queue->_l2state->Producer;    
    i--;
  }
  //While the queue is empty and i < n
  while (head == tail && i != 0);
  
  return 0; //fail queue is empty
}

//spin block in the L2 atomic queue till there is a message. fail and
//return after n iterations
int L2AtomicQueue2QSpinWait (L2AtomicQueue    * queue0,
			     L2AtomicQueue    * queue1,
			     int                n)
{
  if (!L2AtomicQueueEmpty(queue0))
    return 0;  //queue0 is not empty so return
  
  if (!L2AtomicQueueEmpty(queue1))
    return 0;  //queue is not empty so return  

  uint64_t head0, tail0;
  uint64_t head1, tail1;
  
  head0 = queue0->_l2state->Consumer;  
  head1 = queue1->_l2state->Consumer;
  
  size_t i = n;
  do {
    tail0 = queue0->_l2state->Producer;    
    tail1 = queue1->_l2state->Producer;    
    i --;
  } while (head0==tail0 && head1==tail1 && i!=0);   
 
  return 0; 
}



#endif
