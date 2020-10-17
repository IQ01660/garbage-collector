// ==============================================================================
/**
 * bf-gc.c
 **/
// ==============================================================================



// ==============================================================================
// INCLUDES

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "gc.h"
#include "safeio.h"
// ==============================================================================



// ==============================================================================
// TYPES AND STRUCTURES

/** The header for each allocated object. */
typedef struct header {

  /** Pointer to the next header in the list. */
  struct header* next;

  /** Pointer to the previous header in the list. */
  struct header* prev;

  /** The usable size of the block (exclusive of the header itself). */
  size_t         size;

  /** Is the block allocated or free? */
  bool           allocated;

  /** Whether the block has been visited during reachability analysis. */
  bool           marked;

  /** A map of the layout of pointers in the object. */
  gc_layout_s*   layout;

} header_s;

/** A link in a linked stack of pointers, used during heap traversal. */
typedef struct ptr_link {

  /** The next link in the stack. */
  struct ptr_link* next;

  /** The pointer itself. */
  void* ptr;

} ptr_link_s;
// ==============================================================================



// ==============================================================================
// MACRO CONSTANTS AND FUNCTIONS

/** The system's page size. */
#define PAGE_SIZE sysconf(_SC_PAGESIZE)

/**
 * Macros to easily calculate the number of bytes for larger scales (e.g., kilo,
 * mega, gigabytes).
 */
#define KB(size)  ((size_t)size * 1024)
#define MB(size)  (KB(size) * 1024)
#define GB(size)  (MB(size) * 1024)

/** The virtual address space reserved for the heap. */
#define HEAP_SIZE GB(2)

/** Given a pointer to a header, obtain a `void*` pointer to the block itself. */
#define HEADER_TO_BLOCK(hp) ((void*)((intptr_t)hp + sizeof(header_s)))

/** Given a pointer to a block, obtain a `header_s*` pointer to its header. */
#define BLOCK_TO_HEADER(bp) ((header_s*)((intptr_t)bp - sizeof(header_s)))
// ==============================================================================


// ==============================================================================
// GLOBALS

/** The address of the next available byte in the heap region. */
static intptr_t free_addr  = 0;

/** The beginning of the heap. */
static intptr_t start_addr = 0;

/** The end of the heap. */
static intptr_t end_addr   = 0;

/** The head of the free list. */
static header_s* free_list_head = NULL;

/** The head of the allocated list. */
static header_s* allocated_list_head = NULL;

/** The head of the root set stack. */
static ptr_link_s* root_set_head = NULL;
// ==============================================================================



// ==============================================================================
/**
 * Push a pointer onto root set stack.
 *
 * \param ptr The pointer to be pushed.
 */
void rs_push (void* ptr) {

  // Make a new link.
  ptr_link_s* link = malloc(sizeof(ptr_link_s));
  if (link == NULL) {
    ERROR("rs_push(): Failed to allocate link");
  }

  // Have it store the pointer and insert it at the front.
  link->ptr    = ptr;
  link->next   = root_set_head;
  root_set_head = link;
  
} // rs_push ()
// ==============================================================================



// ==============================================================================
/**
 * Pop a pointer from the root set stack.
 *
 * \return The top pointer being removed, if the stack is non-empty;
 *         <code>NULL</code>, otherwise.
 */
void* rs_pop () {

  // Grab the pointer from the link...if there is one.
  if (root_set_head == NULL) {
    return NULL;
  }
  void* ptr = root_set_head->ptr;

  // Remove and free the link.
  ptr_link_s* old_head = root_set_head;
  root_set_head = root_set_head->next;
  free(old_head);

  return ptr;
  
} // rs_pop ()
// ==============================================================================



// ==============================================================================
/**
 * Add a pointer to the _root set_, which are the starting points of the garbage
 * collection heap traversal.  *Only add pointers to objects that will be live
 * at the time of collection.*
 *
 * \param ptr A pointer to be added to the _root set_ of pointers.
 */
void gc_root_set_insert (void* ptr) {

  rs_push(ptr);
  
} // root_set_insert ()
// ==============================================================================



// ==============================================================================
/**
 * The initialization method.  If this is the first use of the heap, initialize it.
 */

void gc_init () {

  // Only do anything if there is no heap region (i.e., first time called).
  if (start_addr == 0) {

    DEBUG("Trying to initialize");
    
    // Allocate virtual address space in which the heap will reside. Make it
    // un-shared and not backed by any file (_anonymous_ space).  A failure to
    // map this space is fatal.
    void* heap = mmap(NULL,
		      HEAP_SIZE,
		      PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS,
		      -1,
		      0);
    if (heap == MAP_FAILED) {
      ERROR("Could not mmap() heap region");
    }

    // Hold onto the boundaries of the heap as a whole.
    start_addr = (intptr_t)heap;
    end_addr   = start_addr + HEAP_SIZE;
    free_addr  = start_addr;

    // DEBUG: Emit a message to indicate that this allocator is being called.
    DEBUG("bf-alloc initialized");

  }

} // gc_init ()
// ==============================================================================


// ==============================================================================
// COPY-AND-PASTE YOUR PROJECT-4 malloc() HERE.
//
//   Note that you may have to adapt small things.  For example, the `init()`
//   function is now `gc_init()` (above); the header is a little bit different
//   from the Project-4 one; my `allocated_list_head` may be a slightly
//   different name than the one you used.  Check the details.
void* gc_malloc (size_t size) {

  // initializing the heap if this is the first malloc call
  gc_init();

  // if a block of size 0 is requested return NULL
  if (size == 0) {
    return NULL;
  }

  // make current point to the head of the free list
  header_s* current = free_list_head;

  // make best NULL for now
  // it will get updated as better fitting blocks are found
  header_s* best    = NULL;

  // while we are not at the tail (or end) of the free list
  while (current != NULL) {
    // if the block (in fact its header) that current points to is allocated
    // throw an error as an allocated block should not be in the free list
    // the address of the header of that block will also be in the error message
    if (current->allocated) {
      ERROR("Allocated block on free list", (intptr_t)current);
    }
    
    // if best hasn't been updated yet and if requested memory size is at most the size of the current block
    // then make the best-fit block the one "current" points to
    // if best has already been updated, then also check if the size of the current block is smaller
    // (i.e. a better fit) than the present best block
    if ( (best == NULL && size <= current->size) ||
	 (best != NULL && size <= current->size && current->size < best->size) ) {
      best = current;
    }

    // if at some point the best has been updated and the size of the block it points to
    // is exactly the size user requested, then there is no need to continue traversing the free list
    if (best != NULL && best->size == size) {
      break;
    }
    
    // move on to the next block the current block points to in its header
    current = current->next;
    
  }

  // the pointer to the block to be allocated
  // this will be returned at the end
  void* new_block_ptr = NULL;

  // if a suitable free block has been found in the free list
  if (best != NULL) {

    // HERE WE ARE REMOVING THE BEST BLOCK FROM THE FREE LIST IF IT WAS FOUND

    // if the best block is the head of the list
    // then update the head of the free list to be the block its "next" points to
    if (best->prev == NULL) {
      free_list_head   = best->next;
    } 
    
    // if the best block is not at the head
    // then make its previous neighbor to point to "best" block's next neighbor
    else {
      best->prev->next = best->next;
    }

    // if the best block is not at the tail of the free list
    // then also update its next neighbor, so that its "prev" points to the
    // the "prev" of the best block
    if (best->next != NULL) {
      best->next->prev = best->prev;
    }

    // make the pointers of the best block to not point to any other neighbor blocks
    best->prev       = NULL;
    best->next       = NULL;

    // mark the best block as allocated
    best->allocated = true;

    // we will later on return the pointer to the block not its header - so update that
    new_block_ptr   = HEADER_TO_BLOCK(best);
    
  } 

  // IF A SUTABLE BLOCK WASN'T FOUND - DO POINTER BUMPING
  else {

    // the padding that should be put before the header
    size_t    padding = 16 - ( (free_addr + sizeof(header_s)) % 16 );

    // =====if the above gives us a padding of 16 then make the padding 0
    if (padding == 16)
    {
      padding = 0;
    }
    
    // assign the next available free addr + padding to the address of the header's pointer
    header_s* header_ptr = (header_s*)(free_addr + padding);
    
    // get the block pointer by shifting the header_ptr by the size of header_s
    new_block_ptr = HEADER_TO_BLOCK(header_ptr);

    // because this block will be allocated
    // its header's next, prev should be set to NULL - (it is not in the free list)
    header_ptr->next      = NULL;
    header_ptr->prev      = NULL;

    // put the size requested to be allocated in the header as well
    header_ptr->size      = size;

    // make sure the block is marked as allocated
    header_ptr->allocated = true;
    
    // find the next free address for future pointer bumping
    intptr_t new_free_addr = (intptr_t)new_block_ptr + size;

    // if this updated free addr gets beyond the end of the heap addr
    // then return NULL right here
    if (new_free_addr > end_addr) {

      return NULL;

    } 
    // otherwise update the free address
    else {

      free_addr = new_free_addr;

    }
  }

  //===========================
  //HERE WE ADD ALLOCATED BLOCK TO THE ALLOCATED LIST
  //===========================

  //first let us get the header of the block we are going to allocate
  header_s* new_header_ptr = BLOCK_TO_HEADER(new_block_ptr);

  //make "next" of this header point to the head of the allocated list
  new_header_ptr->next = allocated_list_head;

  //make "prev" of the new_header NULL
  new_header_ptr->prev = NULL;

  //if the head of the list is not NULL then make sure to update its "prev"
  if (allocated_list_head != NULL) {
    allocated_list_head->prev = new_header_ptr;
  }

  //finally update the head of the list as well
  allocated_list_head = new_header_ptr;

  DEBUG("The bloc added into allocated list:", (intptr_t) new_block_ptr);

  // return the pointer to the block of at least the size requested
  return new_block_ptr;

} // gc_malloc ()
// ==============================================================================



// ==============================================================================
// COPY-AND-PASTE YOUR PROJECT-4 free() HERE.
//
//   See above.  Small details may have changed, but the code should largely be
//   unchanged.
void gc_free (void* ptr) {

  // if the ptr is NULL terminate the function right here
  if (ptr == NULL) {
    return;
  }

  // get the pointer to the header of the block we are trying to free
  header_s* header_ptr = BLOCK_TO_HEADER(ptr);

  // if the header isn't marked as allocated
  // then throw an error as this block is free already - you cannot double free
  if (!header_ptr->allocated) {
    ERROR("Double-free: ", (intptr_t)header_ptr);
  }

  //===========================
  //HERE WE TAKE THE BLOCK OUT OF THE ALLOCATED LIST
  //===========================

  //if this block is the head of the allocated list
  if (header_ptr->prev == NULL) {
    //update the head of the list
    allocated_list_head = header_ptr->next;
  }
  else {
    //get to the prev neighbor and update its next
    header_ptr->prev->next = header_ptr->next;
  }

  //if the block is not at the tail
  if (header_ptr->next != NULL) {
    //get to the next neighbor and update its prev
    header_ptr->next->prev = header_ptr->prev;
  }

  DEBUG("The block taken out from the allocated list:", (intptr_t) ptr);
  //===========================

  // make the "next" of this block to point to the current head of the free list
  header_ptr->next = free_list_head;

  // now make this block we are freeing the head of the list
  free_list_head   = header_ptr;

  // make the "prev" of this block to be NULL as it is at the head of the list
  header_ptr->prev = NULL;

  // if the thing our head points to isn't null
  // i.e. if it is not at tht tail of the list
  if (header_ptr->next != NULL) {
    header_ptr->next->prev = header_ptr;
  }

  DEBUG("The block put into free list:", (intptr_t) ptr);

  // mark the header as not allocated
  header_ptr->allocated = false;
  
  
} // gc_free ()
// ==============================================================================



// ==============================================================================
/**
 * Allocate and return heap space for the structure defined by the given
 * `layout`.
 *
 * \param layout A descriptor of the fields
 * \return A pointer to the allocated block, if successful; `NULL` if unsuccessful.
 */

void* gc_new (gc_layout_s* layout) {

  // Get a block large enough for the requested layout.
  void*     block_ptr  = gc_malloc(layout->size);
  header_s* header_ptr = BLOCK_TO_HEADER(block_ptr);

  // Hold onto the layout for later, when a collection occurs.
  header_ptr->layout = layout;
  
  return block_ptr;
  
} // gc_new ()
// ==============================================================================

// ==============================================================================
/**
 * Extract the pointers from a block
 * and push them onto rs_stack
 * parameters:
 *   layout_ptr: ptr to the layout object that the headers contain
 *   block_ptr:  ptr to the block whose neighbors we are finding
 */
void extract_push (gc_layout_s* layout_ptr, void* block_ptr) {

  // get the number of ptrs from the layout object
  unsigned int num_ptrs = layout_ptr->num_ptrs;

  // get the array of offsets for retrieval of those ptrs
  size_t* ptr_offsets = layout_ptr->ptr_offsets;

  // traverse the array of offsets num_ptrs times
  for (unsigned int i = 0; i < num_ptrs; i++)
  {
    // get the offset value
    size_t ptr_offset = ptr_offsets[i];

    // shift addr by that offset to get the pointer to the neighbor block
    void* neighbor_block_ptr = ((void*)((intptr_t)block_ptr + ptr_offset));
    
    // now push this pointer to the root stack
    rs_push(neighbor_block_ptr);

  }

} // extract_push ()
// ==============================================================================



// ==============================================================================
/**
 * Traverse the heap, marking all live objects.
 */

void mark () {

  // WRITE ME.
  //
  //   Adapt the pseudocode from class for a copying collector to real code here
  //   for a non-copying collector.  Do the traversal, using the linked stack of
  //   pointers that starts at `root_set_head`, setting the `marked` field on
  //   each object you reach.
  
  printf("Started the marking process");
  
  // go on marking as long as the root set stack is not empty
  while(root_set_head != NULL)
  {
    // get the ptr to block next up to be considered by popping the root set stack
    void* block_ptr = rs_pop();

    // get the ptr to the header of this block
    header_s* header_ptr = BLOCK_TO_HEADER(block_ptr);

    // if the block hasn't been visited/marked
    if (! header_ptr->marked)
    {
      // then mark the block's header
      header_ptr->marked = true;

      // get the pointer to the layout object of this block
      gc_layout_s* layout_ptr = header_ptr->layout;

      // GET PTRs OF OTHER BLOCKS THAT THIS BLOCK HAS A REFERENCE OF
      // AND PUSH THEM ONTO RS_STACK
      extract_push(layout_ptr, block_ptr);
    }

  }

} // mark ()
// ==============================================================================


// ==============================================================================
/**
 * Traverse the allocated list of objects.  Free each unmarked object;
 * unmark each marked object (preparing it for the next sweep.
 */

void sweep () {

  // WRITE ME
  //
  //   Walk the allocated list.  Each object that is marked is alive, so clear
  //   its mark.  Each object that is unmarked is dead, so free it with
  //   `gc_free()`.

} // sweep ()
// ==============================================================================



// ==============================================================================
/**
 * Garbage collect the heap.  Traverse and _mark_ live objects based on the
 * _root set_ passed, and then _sweep_ the unmarked, dead objects onto the free
 * list.  This function empties the _root set_.
 */

void gc () {

  printf("Calling the GC");

  // Traverse the heap, marking the objects visited as live.
  mark();

  // And then sweep the dead objects away.
  sweep();

  // Sanity check:  The root set should be empty now.
  assert(root_set_head == NULL);
  
} // gc ()
// ==============================================================================
