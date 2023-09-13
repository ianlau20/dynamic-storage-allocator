/*
 * Storage Allocator – Dynamic Storage Allocator for C Programs
 *
 * Adapted from mm-naive.c - The least memory-efficient malloc package
 * provided by University of Utah Computer Science
 * 
 * In my approach, a block is allocated by using an explict free list, adding pages
 * as needed. Blocks also have a packed header and footer.
 * Blocks are coalesced and are freed and reused.
 * Pages are unmapped when emptied.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* always use 16-byte alignment */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

/* rounds up to the nearest multiple of mem_pagesize() */
#define PAGE_ALIGN(size) (((size) + (mem_pagesize()-1)) & ~(mem_pagesize()-1))

typedef struct list_node {
  struct list_node *prev;
  struct list_node *next;
} list_node;

static void set_allocated(void *b, size_t size);
static void addToFreeList(list_node* ptr);
static void removeFromFreeList(list_node* ptr);
static void* extend(size_t s);
static void* createProlog(void *bp);


typedef size_t block_header;
typedef size_t block_footer;


#define OVERHEAD (sizeof(block_header)+sizeof(block_footer))
#define PAGE_OVERHEAD (sizeof(block_header)*2 + sizeof(block_footer))

#define HDRP(bp) ((char *)(bp) - sizeof(block_header))
#define HDRTOPAY(hdr) ((char *)(hdr) + sizeof(block_header))
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))-OVERHEAD)

#define GET_PPREV(p) ((page_node *)(p))->prev

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp)-OVERHEAD))

#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_SIZE(p) (GET(p) & ~0xF)
#define PACK(size,alloc) ((size) | (alloc))

list_node *free_list;


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  // Make room for prolog and the terminator
  size_t init_size = PAGE_ALIGN(PAGE_OVERHEAD+20000);
  void* bp = mem_map(init_size);
  bp = HDRTOPAY(bp);
  bp = HDRTOPAY(bp);

  // Make one big free block, leaving room for terminator
  PUT(HDRP(bp), PACK(init_size - sizeof(block_footer), 0));
  PUT(FTRP(bp), PACK(init_size - sizeof(block_footer), 0));

  // Create terminator
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

  // Initalize Explicit Free List
  free_list = bp;
  free_list->prev = NULL;
  free_list->next = NULL;

  // Create Prolog (makes space for a list_node but that space isn't needed)
  createProlog(bp);

  return 0;
}


// Generate the prolog for a page
void* createProlog(void *bp)
{
  int new_size = ALIGN(0 + OVERHEAD);
  set_allocated(bp, new_size);
  return bp;
}

// Get more memory: At least s bytes
void *extend(size_t s){
  size_t chunk_size = PAGE_ALIGN(s+PAGE_OVERHEAD+80000);
  void *bp = mem_map(chunk_size);
  bp = HDRTOPAY(bp);
  bp = HDRTOPAY(bp);

  // Make one big free block, leaving room for terminator
  PUT(HDRP(bp), PACK(chunk_size - sizeof(block_footer), 0));
  PUT(FTRP(bp), PACK(chunk_size - sizeof(block_footer), 0));

  // Create terminator
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

  // Attach new block to end of Free_List
  addToFreeList(bp);

  // Create prolog and get next open bp back
  bp = createProlog(bp);
  bp = NEXT_BLKP(bp);

  // Pass the pointer to mm_malloc
  return bp;
}



/* 
 * mm_malloc - Allocate a block by find a big enough block in the
 *     explicit free list, grabbing a new page if necessary.
 */
void *mm_malloc(size_t size)
{
  // Make sure block at least has room for list node
  if(sizeof(list_node) > size){
    size = sizeof(list_node);
  }
  int new_size = ALIGN(size + OVERHEAD);

  /* Iterate through Free List to find first fit */
  void* searcher = free_list;
  while (searcher != NULL){
    // Check if its big enough (currently double checking for allocation)
    if (GET_SIZE(HDRP(searcher)) >= new_size) {
      // Allocate from that payload pointer and return it
      set_allocated(searcher, new_size);
      return searcher;
    }

    // Otherwise we skip to the next thing in the explicit free list
    searcher = ((list_node*) searcher)->prev;
  }

  // If whole list searched, create new page via extend 
  // and allocate in the new page
  void* bp;
  bp = extend(new_size);
  set_allocated(bp, new_size);

  return bp;
}



// Mark a Block as allocated
void set_allocated(void *bp, size_t size){
  // Find out how much space would be left over
  size_t extra_size = GET_SIZE(HDRP(bp)) - size;

  removeFromFreeList((list_node*)bp);

  // Mark Block as Allocated & Keep Size
  PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
  PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));

  // If extra space can accommodate a req. for one byte
  if (extra_size > ALIGN(1 + OVERHEAD)){
    // Mark Block as Allocated & Change Size
    PUT(HDRP(bp), PACK(size, 1));
    PUT(FTRP(bp), PACK(size, 1));

    // Add leftovers to top of Free List
    addToFreeList((list_node*) NEXT_BLKP(bp));

    // Set size of next pointer to the size that was leftover and mark it as unallocated
    PUT(HDRP(NEXT_BLKP(bp)), PACK(extra_size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(extra_size, 0));
  }
}

// Add an item to the explicit free list
void addToFreeList(list_node* ptr){
  if (free_list != NULL){
    free_list->next = ptr;
  }

  ptr->prev = free_list;
  ptr->next = NULL;

  free_list = ptr;
}

// Remove an item from the explicit free list
void removeFromFreeList(list_node* ptr){
  list_node* prevNode = ptr->prev;
  list_node* nextNode = ptr->next;

  // Update Explicit Free List
  // Only List Member
  if (prevNode == NULL && nextNode == NULL){
    free_list = NULL;
  }
  // Oldest List Member
  else if (prevNode == NULL && nextNode != NULL){
    nextNode->prev = NULL;
  }
  // Newest List Member
  else if (prevNode != NULL && nextNode == NULL){
    prevNode->next = NULL;
    free_list = prevNode;
  }
  // Interior List Member
  else if (prevNode != NULL && nextNode != NULL){
    prevNode->next = nextNode;
    nextNode->prev = prevNode;
  }

  return;
}


// Coalesce adjacent free blocks
void *coalesce(void *bp){
  size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  // No free adj blocks
  if (prev_alloc && next_alloc){
    addToFreeList((list_node *)bp);
  }
  // The following block is free
  else if (prev_alloc && !next_alloc) {
    removeFromFreeList((list_node *)NEXT_BLKP(bp));
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    addToFreeList((list_node *)bp);
  }
  // Previous block is free
  else if (!prev_alloc && next_alloc) {
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  // Both adjacent blocks are free
  else {
    removeFromFreeList((list_node *)NEXT_BLKP(bp));
    size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
    
  return bp;
}

/*
 * mm_free - Freeing a block changes the block's header's allocation bit to 0
 * and adds the block to the end of the free_list. Performs coalescing and 
 * unmaps pages if necessary.
 */
void mm_free(void *ptr)
{
  // Get the block pointer and set the allocation bit to zero
  PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 0));

  // Coalesce
  ptr = coalesce(ptr);

  // Remove Page if Emptied
  if(GET_SIZE(HDRP(NEXT_BLKP(ptr))) == 0 && GET_SIZE(HDRP(HDRP(ptr))) == ALIGN(OVERHEAD)){
    size_t sizeToRelease = GET_SIZE(HDRP(ptr)) + PAGE_OVERHEAD + sizeof(block_header);
    removeFromFreeList(ptr);
    ptr = HDRP(HDRP(HDRP(HDRP(ptr))));
    mem_unmap(ptr, sizeToRelease);
  }
}
