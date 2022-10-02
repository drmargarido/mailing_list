#ifndef ALLOCATOR_INCLUDED
#define ALLOCATOR_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <stddef.h>

typedef struct Arena {
  unsigned char * buffer;
  size_t buffer_length;
  size_t offset;
} Arena;

int init_arena(Arena * arena, size_t size){
  arena->offset = 0;
  arena->buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
  if(arena->buffer == NULL){
    printf("Failed to allocate memory for the arena");
    return 1;
  }
  arena->buffer_length = size;
  return 0;
}

// If initialized with buffer, then it shouldn't be freed
int init_arena_with_buffer(Arena * arena, void * buffer, int size){
  arena->offset = 0;
  arena->buffer = buffer;
  arena->buffer_length = size;
  return 0;
}

void reset_arena(Arena * arena){
  memset(arena->buffer, 0, arena->offset);
  arena->offset = 0;
}

void free_arena(Arena * arena){
  munmap(arena->buffer, arena->buffer_length);
}

void * alloc_in_arena(Arena * arena, size_t size){
  assert(arena->offset + size <= arena->buffer_length);
  void * ptr = &arena->buffer[arena->offset];
  arena->offset += size;
  return ptr;
}

typedef struct PoolFreeNode PoolFreeNode;
struct PoolFreeNode {
  PoolFreeNode * next;
};

typedef struct Pool {
  unsigned char * buffer;
  size_t total_chunks;
  size_t chunk_size;

  PoolFreeNode * head;
} Pool;


int init_pool(Pool * pool, size_t chunk_size, size_t total_chunks){
  pool->buffer = malloc(chunk_size * total_chunks);
  if(pool->buffer == NULL){
    printf("Failed to initialize the memory pool\n");
    return -1;
  }

  pool->total_chunks = total_chunks;
  pool->chunk_size = chunk_size;
  pool->head = NULL;

  // Create linked list with the empty nodes
  for(int i=0; i < pool->total_chunks; i++){
    void * ptr = &pool->buffer[i * pool->chunk_size];
    PoolFreeNode *node = (PoolFreeNode *) ptr;
    node->next = pool->head;
    pool->head = node;
  }
  return 0;
}

void free_pool(Pool * pool){
  free(pool->buffer);
}

void * alloc_in_pool(Pool * pool){
  if(pool->head == NULL){
    printf("The memory pool has no more available memory\n");
    return NULL;
  }

  PoolFreeNode * node = pool->head;
  pool->head = node->next;
  return memset(node, 0, pool->chunk_size);
}

void free_in_pool(Pool * pool, void * ptr){
  PoolFreeNode * node = (PoolFreeNode *) ptr;
  node->next = pool->head;
  pool->head = node;
}

#endif
