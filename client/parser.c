#include "../common/allocator.c"
#include "../vendor/jsmn/jsmn.h"

/*
* Expected json format:
* {
*   "total": int,
*   "games": [
*     {"id": int, "name": string},
*     ...
*   ]
* }
*/
GamesList * parse_get_games(char * response, Arena * arena){
  jsmn_parser p;
  size_t max_tokens = 10000;
  jsmntok_t * t = alloc_in_arena(arena, max_tokens * sizeof(jsmntok_t));

  jsmn_init(&p);
  int r = jsmn_parse(&p, response, strlen(response), t, max_tokens);

  GamesList * list = alloc_in_arena(arena, sizeof(GamesList));

  char total_text[INT_MAX_DIGITS] = "";
  strncat(total_text, &response[t[2].start], t[2].end - t[2].start);
  list->total = atoi(total_text);
  list->games = alloc_in_arena(arena, sizeof(Game) * list->total);

  const int games_start_idx = 5;
  const int game_tokens = 5;
  for(int i=0; i < list->total; i++){
    int id_idx = games_start_idx + (game_tokens * i) + 2;
    char id_text[INT_MAX_DIGITS] = "";
    strncat(id_text, &response[t[id_idx].start], t[id_idx].end - t[id_idx].start);
    list->games[i].id = atoi(id_text);

    int name_idx = games_start_idx + (game_tokens * i) + 4;
    strncat(
      list->games[i].name,
      &response[t[name_idx].start],
      t[name_idx].end - t[name_idx].start
    );
  }

  return list;
}



/*
* Expected json format:
* {
*   "total": int,
*   "emails": [string, string, ...]
* }
*/
EmailsList * parse_get_emails(char * response, Arena * arena){
  jsmn_parser p;
  size_t max_tokens = 10000;
  jsmntok_t * t = alloc_in_arena(arena, max_tokens * sizeof(jsmntok_t));

  jsmn_init(&p);
  int r = jsmn_parse(&p, response, strlen(response), t, max_tokens);

  EmailsList * list = alloc_in_arena(arena, sizeof(EmailsList));

  char total_text[INT_MAX_DIGITS] = "";
  strncat(total_text, &response[t[2].start], t[2].end - t[2].start);
  list->total = atoi(total_text);
  list->emails = alloc_in_arena(arena, sizeof(EMAIL_SIZE) * list->total);

  const int emails_start_idx = 5;
  for(int i=0; i < list->total; i++){
    int idx = emails_start_idx + i;
    memcpy(
      &list->emails[i * EMAIL_SIZE],
      &response[t[idx].start],
      t[idx].end - t[idx].start
    );
    response[t[idx].start + (t[idx].end - t[idx].start)] = '\0';
  }

  return list;
}
