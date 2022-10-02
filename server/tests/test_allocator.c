#include <assert.h>
#include "../../common/allocator.c"

int main(int argc, char **argv){
  Arena arena = {};
  init_arena(&arena, 1024);

  int * num = alloc_in_arena(&arena, sizeof(int));
  *num = 10;
  assert(*num == 10);

  reset_arena(&arena);
  assert(*num == 0);

  // Allocate space over the max size and see if it grows
  char * txt = alloc_in_arena(&arena, 13);
  strncpy(txt, "Hello Friend", 12);

  free_arena(&arena);

  Pool pool = {};
  init_pool(&pool, sizeof(int), 10);

  num = alloc_in_pool(&pool);
  *num = 20;
  assert(*num == 20);

  free_in_pool(&pool, num);
  assert(*num != 20);

  free_pool(&pool);
  return 0;
}
