// ==============================================================================
/**
 * bf-alloc.h
 *
 * The interface for using a _garbage-collected heap_ using _mark-sweep
 * collection_, built upon a _best-fit_ allocator.
 **/
// ==============================================================================



// ==============================================================================
// AVOID MULTIPLE INCLUSION

#if !defined (_GC_H)
#define _GC_H
// ==============================================================================



// ==============================================================================
// INCLUDES

#include <stdint.h>
#include <stdlib.h>
// ==============================================================================



// ==============================================================================
// TYPES AND STRUCTURES

/**
 * The description of a heap object, as needed by the GC to find the pointers.
 */
typedef struct gc_layout {

  /** The size of the object, in bytes. */
  size_t size;

  /** The number of pointers in the object. */
  unsigned int num_ptrs;

  /** The offsets into the object at which pointers reside. */
  size_t* ptr_offsets;
  
} gc_layout_s;
// ==============================================================================



// ==============================================================================
// FUNCTIONS

/**
 * Allocate and return heap space for the structure defined by the given
 * `layout`.
 *
 * \param layout A descriptor of the fields
 * \return A pointer to the allocated block, if successful; `NULL` if unsuccessful.
 */
void* gc_new (gc_layout_s* layout);

/**
 * Garbage collect the heap.  Traverse and _mark_ live objects based on the
 * _root set_ passed, and then _sweep_ the unmarked, dead objects onto the free
 * list.  This function empties the _root set_.
 */
void gc ();

/**
 * Add a pointer to the _root set_, which are the starting points of the garbage
 * collection heap traversal.  *Only add pointers to objects that will be live
 * at the time of collection.*
 *
 * \param ptr A pointer to be added to the _root set_ of pointers.
 */
void gc_root_set_insert (void* ptr);
// ==============================================================================



// ==============================================================================
#endif // !defined (_GC_H)
// ==============================================================================
