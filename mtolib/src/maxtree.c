#include "maxtree.h"

#include <assert.h>

#define MT_INDEX_OF(PIXEL) (((PIXEL).location.depth * mt->img.depth) + \
((PIXEL).location.y * mt->img.width) + (PIXEL).location.x)

const int mt_conn_12[MT_CONN_12_HEIGHT * MT_CONN_12_WIDTH] =
{
  0, 0, 1, 0, 0,
  0, 1, 1, 1, 0,
  1, 1, 0, 1, 1,
  0, 1, 1, 1, 0,
  0, 0, 1, 0, 0
};

const int mt_conn_8[MT_CONN_8_HEIGHT * MT_CONN_8_WIDTH] =
{
  1, 1, 1,
  1, 0, 1,
  1, 1, 1,
};

const int mt_conn_4[MT_CONN_4_HEIGHT * MT_CONN_4_WIDTH] =
{
  0, 1, 0,
  1, 0, 1,
  0, 1, 0,
};

const int mt_conn_6[MT_CONN_6_DEPTH * MT_CONN_6_HEIGHT * MT_CONN_6_WIDTH] =
{
  0, 0, 0,
  0, 1, 0,
  0, 0, 0,

  0, 1, 0,
  1, 0, 1,
  0, 1, 0,

  0, 0, 0,
  0, 1, 0,
  0, 0, 0
};


mt_pixel mt_starting_pixel(mt_data* mt)
{
  // Find the minimum pixel value in the image
  SHORT_TYPE y;
  SHORT_TYPE x;
  SHORT_TYPE depth;

  mt_pixel pixel;
  pixel.location.x = 0;
  pixel.location.y = 0;
  pixel.location.depth = 0;

  pixel.value = mt->img.data[0];

  // iterate over image pixels
  for (depth = 0; depth != mt->img.depth; ++depth)
  {
    for (y = 0; y != mt->img.height; ++y)
    {
      for (x = 0; x != mt->img.width; ++x)
      {
      // Convert from x,y coordinates to an array index
      // NOTE: because it is a 1D array? I don't really get how this iterates over all indices?
        INT_TYPE index = (depth * mt->img.depth) + (y * mt->img.width) + x;

        // If the pixel is less than the current minimum, update the minimum
        if (mt->img.data[index] < pixel.value)
        {
          pixel.value = mt->img.data[index];
          pixel.location.x = x;
          pixel.location.y = y;
          pixel.location.depth = depth;
        }
      }
    }
  }

  return pixel;
}

static void mt_init_nodes(mt_data* mt)
{
  // Initialise the nodes of the maxtree
  INT_TYPE i;

  // Set all parents as unassigned, and all areas as 1
  for (i = 0; i != mt->img.size; ++i)
  {
    mt->nodes[i].parent = MT_UNASSIGNED;
    mt->nodes[i].area = 1;
  }
}

static int mt_queue_neighbour(mt_data* mt, PIXEL_TYPE val,
  SHORT_TYPE x, SHORT_TYPE y, SHORT_TYPE depth)
{
  // Add a pixel to the queue for processing

  //Create a pixel and set its location
  mt_pixel neighbour;
  neighbour.location.x = x;
  neighbour.location.y = y;
  neighbour.location.depth = depth;

  // Convert from x,y,depth coordinates to an array index
  INT_TYPE neighbour_index = MT_INDEX_OF(neighbour);
  // Get a pointer to the neighbour
  mt_node *neighbour_node = mt->nodes + neighbour_index;

  // NOTE: why is the neighbor the parent?
  // If the neighbour has not already been processed, add it to the queue
  if (neighbour_node->parent == MT_UNASSIGNED)
  {
    neighbour.value = mt->img.data[neighbour_index];
    neighbour_node->parent = MT_IN_QUEUE;
    mt_heap_insert(&mt->heap, &neighbour);

    // NOTE: added neighbor (not a loop, just if statement) to the queue that are lower in intensity than (or higher in the case of this one) until a neighbor is found with a higher intensity in the connectivity region?
    // If the neighbour has a higher value than the current node, return 1
    if (neighbour.value > val)
    {
      return 1;
    }
  }

  return 0;
}

static void mt_queue_neighbours(mt_data* mt,
  mt_pixel* pixel)
{
  // NOTE: conn = connectivity? apparently it refers to coordinate of connectivity grid? (defined further below)
  // Seems to be limiting conn values within image coordinates

  // Radius is half size of connectivity
  INT_TYPE radius_y = mt->connectivity.height / 2;
  INT_TYPE radius_x = mt->connectivity.width / 2;
  INT_TYPE radius_depth = mt->connectivity.depth / 2;

  // If pixel's x is less than radius, conn = the difference
  INT_TYPE conn_x_min = 0;
  if (pixel->location.x < radius_x)
    conn_x_min = radius_x - pixel->location.x;

  // Ditto for y
  INT_TYPE conn_y_min = 0;
  if (pixel->location.y < radius_y)
    conn_y_min = radius_y - pixel->location.y;
  
  // Ditto for depth
  INT_TYPE conn_depth_min = 0;
  if (pixel->location.depth < radius_depth)
    conn_depth_min = radius_depth - pixel->location.depth;

  // NOTE: avoid going outside image bounds (so basically accounting for edge effects?)
  // If pixel's x + radius > image width, conn = radius + width - location - 1
  INT_TYPE conn_x_max = 2 * radius_x;
  if (pixel->location.x + radius_x >= mt->img.width)
    conn_x_max = radius_x + mt->img.width - pixel->location.x - 1;

  INT_TYPE conn_y_max = 2 * radius_y;
  if (pixel->location.y + radius_y >= mt->img.height)
    conn_y_max = radius_y + mt->img.height - pixel->location.y - 1;
  
  INT_TYPE conn_depth_max = 2 * radius_depth;
  if (pixel->location.depth + radius_depth >= mt->img.depth)
    conn_depth_max = radius_depth + mt->img.depth - pixel->location.depth - 1;

  // Conn coordinates refer to position with connectivity grid
  // NOTE: check within the connectivity region
  INT_TYPE conn_depth;
  for (conn_depth = conn_depth_min; conn_depth <= conn_depth_max; ++conn_depth)
  {
    INT_TYPE conn_y;
    for (conn_y = conn_y_min; conn_y <= conn_y_max; ++conn_y)
    {
      INT_TYPE conn_x;
      for (conn_x = conn_x_min; conn_x <= conn_x_max; ++conn_x)
      {
      // Skip iteration if 0 in connectivity grid
        if (mt->connectivity.
          neighbors[(conn_depth * mt->connectivity.depth) + \
          (conn_y * mt->connectivity.width) + conn_x] == 0)
        {
          continue;
        }

        // Try to queue neighbour at x = x-rad+conn
        // If successfully queued and value higher than current,
        // break out of function
        if (mt_queue_neighbour(mt, pixel->value,
          pixel->location.x - radius_x + conn_x,
          pixel->location.y - radius_y + conn_y,
          pixel->location.depth - radius_depth + conn_depth))
        {
          return;
        }
      }
    }
  }
}

static void mt_merge_nodes(mt_data* mt,
  INT_TYPE merge_to_idx,
  INT_TYPE merge_from_idx)
{
  // Merge two nodes
  // NOTE: meaning what exactly?

  // NOTE: does adding the index get you the value of the specific nodes???
  mt_node *merge_to = mt->nodes + merge_to_idx;
  mt_node_attributes *merge_to_attr = mt->nodes_attributes +
    merge_to_idx;

  mt_node *merge_from = mt->nodes + merge_from_idx;
  mt_node_attributes *merge_from_attr = mt->nodes_attributes +
    merge_from_idx;

  // NOTE: the merge_to node is basically the one being updated?
  merge_to->area += merge_from->area;

  // NOTE: this is the delta in terms of gray level. This effectively "soft" binarizes ("quantizes") the image, see Salembier 1998?
  FLOAT_TYPE delta = mt->img.data[merge_from_idx] -
    mt->img.data[merge_to_idx];

  // NOTE: what the hell does this mean/do? It doesn't look like the power attribute I know? and why does the merge one equal this, and the merge to is += merge_from?
  merge_from_attr->power += delta *
    (2 * merge_from_attr->volume + delta * merge_from->area);
  merge_to_attr->power += merge_from_attr->power;

  // NOTE: I guess it makes sense to be "volume" because you have the 2 dimensions from the x,y pixels and an additional one from the 'delta' in graylevel? seems to be needed for power_attribute calculation.
  merge_from_attr->volume += delta * merge_from->area;
  merge_to_attr->volume += merge_from_attr->volume;
}

static void mt_descend(mt_data* mt, mt_pixel *next_pixel)
{
  // NOTE: remove current pixel from the stack 
  mt_pixel old_top = *mt_stack_remove(&mt->stack);
  INT_TYPE old_top_index = MT_INDEX_OF(old_top);


  mt_pixel* stack_top = MT_STACK_TOP(&mt->stack);

  // NOTE: if at a lower gray level, insert next pixel on the stack
  if (stack_top->value < next_pixel->value)
  {
    mt_stack_insert(&mt->stack, next_pixel);
  }

  stack_top = MT_STACK_TOP(&mt->stack);
  INT_TYPE stack_top_index = MT_INDEX_OF(*stack_top);

  // NOTE: why set nodes[top index] to top stack index of new/next pixel (if inserted just prior) or the value prior (if not inserted)?
  mt->nodes[old_top_index].parent = stack_top_index;
  mt_merge_nodes(mt, stack_top_index, old_top_index);
}

static void mt_remaining_stack(mt_data* mt)
{
  while (MT_STACK_SIZE(&mt->stack) > 1)
  {
    mt_pixel old_top = *mt_stack_remove(&mt->stack);
    INT_TYPE old_top_index = MT_INDEX_OF(old_top);

    mt_pixel* stack_top = MT_STACK_TOP(&mt->stack);
    INT_TYPE stack_top_index = MT_INDEX_OF(*stack_top);

    mt->nodes[old_top_index].parent = stack_top_index;
    mt_merge_nodes(mt, stack_top_index, old_top_index);
  }
}

void mt_flood(mt_data* mt)
{
  // NOTE: just make sure the connectivity is okay
  assert(mt->connectivity.height > 0);
  assert(mt->connectivity.height % 2 == 1);
  assert(mt->connectivity.width > 0);
  assert(mt->connectivity.width % 2 == 1);
  assert(mt->connectivity.depth > 0);
  assert(mt->connectivity.depth % 2 == 1);

  if (mt->verbosity_level)
  {
    int num_neighbors = 0;
    int i;
    for (i = 0; i != mt->connectivity.depth; i++)
    {
      int j;
      for (j = 0; j != mt->connectivity.height; ++j)
      {
        int k;
        for (k = 0; k != mt->connectivity.width; ++k)
        {
          // NOTE should it maybe be [(i * mt->connectivity.depth) * (j * mt->connectivity.height) + k]???
          if (mt->connectivity.neighbors[(i * mt->connectivity.depth) + (j * mt->connectivity.height) + k])
          {
            ++num_neighbors;
          }
        }
      }
    }

    printf("%d neighbors connectivity.\n", num_neighbors);
  }

  // NOTE: just find the minimum pixel to start flooding from?
  mt_pixel next_pixel = mt_starting_pixel(mt);
  // NOTE: find the "next" index??? just in the 1D array of image?
  INT_TYPE next_index = MT_INDEX_OF(next_pixel);
  // NOTE: initialise root note/flooding algorithm?
  mt->root = mt->nodes + next_index;
  mt->nodes[next_index].parent = MT_NO_PARENT;
  // NOTE: insert the first pixel on both the heap and stack?
  mt_heap_insert(&mt->heap, &next_pixel);
  mt_stack_insert(&mt->stack, &next_pixel);

  // NOTE: continue until the heap is empty why? does that mean every pixel has been visited/dealt with?
  while (MT_HEAP_NOT_EMPTY(&mt->heap))
  {
    // NOTE: initiate going to the next pixel (from heap)
    mt_pixel pixel = next_pixel;
    INT_TYPE index = next_index;

    // NOTE: what does queueing a neighbor mean? seems to just mean adding to the heap? if that is the case, is it the order in which it is added that has something to do with connectivity/maxtree branches?
    mt_queue_neighbours(mt, &pixel);

    // NOTE: go to the next pixel (from heap)
    next_pixel = *MT_HEAP_TOP(&mt->heap);
    next_index = MT_INDEX_OF(next_pixel);

    // NOTE: put on the stack if higher graylevel?
    if (next_pixel.value > pixel.value)
    {
      // Higher level

      mt_stack_insert(&mt->stack, &next_pixel);
      continue;
    }

    // NOTE: if a pixel is added on the stack, it means it was at the current graylevel? and can thus be removed from the heap
    pixel = *mt_heap_remove(&mt->heap);
    // NOTE: index of the last entry in the heap (after the one before just got removed)
    index = MT_INDEX_OF(pixel);
    mt_pixel *stack_top = MT_STACK_TOP(&mt->stack);
    INT_TYPE stack_top_index = MT_INDEX_OF(*stack_top);

    // NOTE: after removing from heap, do the indices match with those of the stack?
    if (index != stack_top_index)
    {
      // NOTE: set the stack_top_index as parent makes sense how?
      mt->nodes[index].parent = stack_top_index;
      // NOTE: increase the area by 1?
      ++mt->nodes[stack_top_index].area;
    }

    // NOTE: does this mean all pixels are done?
    if (MT_HEAP_EMPTY(&mt->heap))
    {
      break;
    }

    // NOTE: grab the next pixel from the top of the heap
    next_pixel = *MT_HEAP_TOP(&mt->heap);
    next_index = MT_INDEX_OF(next_pixel);

    // NOTE: lower graylevel
    if (next_pixel.value < pixel.value)
    {
      // Lower level

      // NOTE: pop current pixel from the stack?????
      mt_descend(mt, &next_pixel);
    }
  }

  mt_remaining_stack(mt);

  mt_stack_free_entries(&mt->stack);
  mt_heap_free_entries(&mt->heap);
}

void mt_init(mt_data* mt, const image* img)
{
  mt->img = *img;

  mt->nodes = safe_malloc(mt->img.size * sizeof(mt_node));
  // NOTE: this stores attirbuteinfo (e.g. volume/power) for each pixel.
  mt->nodes_attributes = safe_calloc(mt->img.size,
    sizeof(mt_node_attributes));

  // NOTE: initialise and allocate memory for idk really?
  mt_stack_alloc_entries(&mt->stack);
  mt_heap_alloc_entries(&mt->heap);

  // NOTE: initialise area and parent node to defaults
  mt_init_nodes(mt);

  // NOTE: needed for what? the flooding it seems, but how exactly?
  mt->connectivity.neighbors = mt_conn_6;
  mt->connectivity.width = MT_CONN_6_WIDTH;
  mt->connectivity.height = MT_CONN_6_HEIGHT;
  mt->connectivity.depth = MT_CONN_6_DEPTH;

  mt->verbosity_level = 0;
}

void mt_free(mt_data* mt)
{
  // Free the memory occupied by the max tree
  free(mt->nodes);
  free(mt->nodes_attributes);

  //memset(mt, 0, sizeof(mt_data));
}

void mt_set_verbosity_level(mt_data* mt, int verbosity_level)
{
  mt->verbosity_level = verbosity_level;
}
