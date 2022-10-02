#include <stdlib.h>
#include "../common/allocator.c"
#include "../common/structures.c"

char * games_list_to_json(GamesList * list, Arena * arena){
  size_t game_size = INT_MAX_DIGITS + GAME_NAME_SIZE;
  size_t max_len = (INT_MAX_DIGITS + list->total * game_size) * 1.5;
  char * games_text = alloc_in_arena(arena, max_len);
  char * json = alloc_in_arena(arena, max_len);
  int pos = 0;
  for(int i=0; i < list->total; i++){
    if(i > 0){
      strcat(games_text, ",");
      pos++;
    }

    Game * g = &list->games[i];
    int written = snprintf(&games_text[pos], game_size * 1.5, "{\"id\": %d, \"name\": \"%s\"}", g->id, g->name);
    pos += written;
  }

  snprintf(json, max_len, "{\"total\": %d, \"games\": [%s]}", list->total, games_text);
  return json;
}

char * emails_list_to_json(RegistrationsList * list, Arena * arena){
  size_t max_len = EMAIL_SIZE * list->total * 1.5;
  char * json = alloc_in_arena(arena, max_len);
  snprintf(json, 100, "{\"total\": %d, \"emails\":[", list->total);
  for(int i=0; i < list->total; i++){
    if(i > 0){
      strncat(json, ",", 2);
    }
    strncat(json, "\"", 2);
    strncat(json, list->registrations[i].email, EMAIL_SIZE);
    strncat(json, "\"", 2);
  }
  strncat(json, "]}", 3);
  return json;
}
