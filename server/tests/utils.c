#include <stdlib.h>
#include "../../common/allocator.c"

char random_char(){
  return (rand() % 25) + 97;
}

char * random_email(Arena * arena){
  char * email = alloc_in_arena(arena, EMAIL_SIZE);
  snprintf(
    email, 20, "%c%c%c%c%c%c%c%c%c@gmail.com",
    random_char(), random_char(), random_char(), random_char(), random_char(),
    random_char(), random_char(), random_char(), random_char()
  );
  return email;
}

char * random_game(Arena * arena){
  char * name = alloc_in_arena(arena, GAME_NAME_SIZE);
  snprintf(name, 10, "%c%c%c%c%c%c%c%c%c",
    random_char(), random_char(), random_char(), random_char(), random_char(),
    random_char(), random_char(), random_char(), random_char()
  );
  return name;
}
