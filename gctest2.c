#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "gc.h"

typedef struct link {
  int          value;
  struct link* next;
  struct link* prev;
} link_s;

void print_list (char* label, link_s* head) {

  printf("%s\n", label);
  link_s* current = head;
  while (current != NULL) {
    printf("\tvalue = %d\tlink @%p\n", current->value, current);
    current = current->next;
  }
  
} // print_list ()

link_s* add (link_s* head, link_s* new_link) {

  assert(new_link != NULL);
  new_link->prev = NULL;
  if (head != NULL) {
    head->prev     = new_link;
  }
  new_link->next = head;
  head           = new_link;

  return head;
  
}

link_s* del (link_s* current) {

  assert(current != NULL);
  link_s* next = current->next;
  if (current->next != NULL) {
    current->next->prev = current->prev;
  }
  if (current->prev != NULL) {
    current->prev->next = current->next;
  }

  return next;
  
}

int main (int argc, char** argv) {

  // Check usage and extract the command line argument(s).
  if (argc != 2) {
    fprintf(stderr, "USAGE: %s <number of objects>\n", argv[0]);
    return 1;
  }
  int num_objs = atoi(argv[1]);

  // Define what a link object looks like to the GC.
  gc_layout_s* link_layout    = malloc(sizeof(gc_layout_s));
  assert(link_layout != NULL);
  link_layout->size           = sizeof(link_s);
  link_layout->num_ptrs       = 2;
  link_layout->ptr_offsets    = malloc(sizeof(size_t) * link_layout->num_ptrs);
  assert(link_layout->ptr_offsets != NULL);
  link_layout->ptr_offsets[0] = offsetof(link_s, next);
  link_layout->ptr_offsets[1] = offsetof(link_s, prev);

  // Create a list of arbitrary integers.
  link_s* head    = NULL;
  link_s* current = NULL;
  for (int i = 0; i < num_objs; i += 1) {
    link_s* current = gc_new(link_layout);
    assert(current != NULL);
    current->value = i;
    head = add(head, current);
  }

  // Show everything, collect, and then show it all again.
  print_list("Initial:", head);
  gc_root_set_insert(head);
  gc();
  print_list("Collected:", head);

  // Remove the first element, collect, and then add it back in.  Is
  // it the same?
  head = head->next;
  head->prev = NULL;
  gc_root_set_insert(head);
  gc();
  current = gc_new(link_layout);
  assert(current != NULL);
  head = add(head, current);
  print_list("Pop and push:", head);

  // Walk halfway into the list, and make that the start of the new
  // list.
  current = head;
  for (int i = 0; i < num_objs / 2; i += 1) {
    assert(current != NULL);
    current = current->next;
  }
  current->prev = NULL;
  head = current;
  gc_root_set_insert(head);
  gc();
  print_list("Disconnected:", head);

  // Remove every other link.
  current = head;
  int i = 0;
  while (current != NULL) {
    if (i % 2 == 0) {
      current = current->next;
    } else {
      current = del(current);
    }
    i += 1;
  }
  gc_root_set_insert(head);
  gc();
  print_list("Every other:", head);

  // Empty root set.
  gc();

  return 0;
  
} // main ()
