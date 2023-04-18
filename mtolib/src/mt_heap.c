#include "mt_heap.h"

// Heap

void mt_heap_alloc_entries(mt_heap* heap)
{
  heap->entries =
    safe_malloc(MT_HEAP_NUM_ENTRIES_BYTES(MT_HEAP_INIT_SIZE));
  heap->num_entries = 0;
  heap->max_entries = MT_HEAP_INIT_SIZE; 
}

void mt_heap_free_entries(mt_heap* heap)
{
  free(heap->entries);
  
  heap->entries = NULL;
}

void mt_heap_resize(mt_heap* heap)
{
  heap->max_entries <<= 1;
  
  heap->entries = safe_realloc(heap->entries,
    MT_HEAP_NUM_ENTRIES_BYTES(heap->max_entries));
}

void mt_heap_insert(mt_heap* heap, const mt_pixel* pixel)
{
  // NOTE: if the stack overflows, just resize it?
  if (heap->max_entries == heap->num_entries)
  {
    mt_heap_resize(heap);
  }
  
  // NOTE: you're adding one entry so + 1?
  INT_TYPE index = heap->num_entries + 1;
  // NOTE: why + index and not + 1?
  mt_pixel* entry = heap->entries + index;
  PIXEL_TYPE pix_value = pixel->value;
  
  // Up-heap.
  
  // NOTE: do not do "this"? for first insert? should this not just be an if statement?
  while (index != 1)  
  {
    // NOTE: bitshift up 1 and call that up_index
    INT_TYPE up_index = MT_HEAP_UP(index);
    // NOTE: why is it + up_index and not + 1?
    mt_pixel* up_entry = heap->entries + up_index;
    
    // NOTE: break for what reason? should the while loop just have been an if statement? or do you expect to do multiple iterations?
    if (up_entry->value >= pix_value)
    {
      break;
    }
    
    // NOTE: update current entry/indices to "up" ones
    *entry = *up_entry;
    
    index = up_index;
    entry = up_entry;
  }
  
  *entry = *pixel;
  
  ++heap->num_entries; 
}

const mt_pixel* mt_heap_remove(mt_heap* heap)
{
  INT_TYPE index = 1;                         
  mt_pixel* entry = heap->entries + index;    
  mt_pixel root_entry = *entry;               // Save root entry.
  
                                              // Entry to down-heap.
  mt_pixel* last_entry = heap->entries + heap->num_entries;
  
  // Down-heap.
  
  while (TRUE)
  {
    // NOTE: bitshift down a graylevel?
    index = MT_HEAP_DOWN(index, 0);
    
    // NOTE: stop shifting down until you are more than number of heap entries??? is this only (maybe) hit the first time, since it seems like you would always decrease index in this while loop?
    if (index > heap->num_entries)
    {
      break;
    }
    
    // NOTE: what does (left) branch mean here, and how does "adding" 'index' number of members to the heap entries and putting that in down (which is of type pixel?) make sense???
    mt_pixel* down = heap->entries + index; // Left branch.
    
    if (index < heap->num_entries)
    {
      // NOTE: why is adding just 1 means right branch, whilst adding index (which could be 1 as well right?) the left branch?
      mt_pixel* down_right = down + 1;      // Right branch.

      // NOTE: if the right branch is larger than left branch
      if (down_right->value > down->value)
      {
        ++down;
        ++index;
      }
    }      
    // NOTE: if the left branch is less than or equal to last entry to down (left?) heap?
    if (down->value <= last_entry->value)
    {
      break;
    }
    
    *entry = *down;
    entry = down;
  }
  
  *entry = *last_entry;
  *last_entry = root_entry;
  
  // NOTE: this actually reads as --(heap->num_entries) because of operator precedence. So really just removing one entry from the heap (the function's job)
  --heap->num_entries;
    
  return last_entry;
}
