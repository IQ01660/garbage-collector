#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "gc.h"

int main (int argc, char** argv) {

  // Check usage and extract the command line argument(s).
  if (argc != 2) {
    fprintf(stderr, "USAGE: %s <number of objects>\n", argv[0]);
    return 1;
  }
  int num_objs = atoi(argv[1]);

  // Define what an int object looks like to the GC.
  gc_layout_s* int_layout = malloc(sizeof(gc_layout_s));
  assert(int_layout != NULL);
  int_layout->size        = sizeof(int);
  int_layout->num_ptrs    = 0;
  int_layout->ptr_offsets = NULL;

  // Make an array of pointers to int objects.  Define the array.
  gc_layout_s* array_layout = malloc(sizeof(gc_layout_s));
  assert(array_layout != NULL);
  array_layout->size        = sizeof(int*) * num_objs;
  array_layout->num_ptrs    = num_objs;
  array_layout->ptr_offsets = malloc(sizeof(size_t) * num_objs);
  assert(array_layout->ptr_offsets != NULL);
  for (int i = 0; i < num_objs; i += 1) {
    array_layout->ptr_offsets[i] = i * sizeof(int*);
  }
  
  int** x = gc_new(array_layout);
  assert(x != NULL);
  for (int i = 0; i < num_objs; i += 1) {
    x[i]  = gc_new(int_layout);
    *x[i] = i; // Make each int hold a value.
  }

  gc_root_set_insert(x);
  gc();

  return 0;
  
} // main ()
