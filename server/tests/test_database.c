#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "../../common/structures.c"
#include "../../common/allocator.c"
#include "../database.c"
#include "utils.c"
#include "../secrets.c"

#define KB 1024
#define MB (KB * 1024)

int main(){
  /* Intializes random number generator */
  time_t t;
  srand((unsigned) time(&t));

  const char *conn_string = DATABASE_CONNECTION_STRING;
  DB db = {};
  int result = init_db(&db, conn_string);
  if(result != 0){
    return -1;
  }

  Arena arena = {};
  result = init_arena(&arena, 10 * MB);
  if(result != 0){
    close_db(&db);
    return -2;
  }

  result = create_game(&db, random_game(&arena));
  assert(result == 0);

  GamesList list;
  result = read_games(&db, &arena, &list);
  assert(result == 0);
  assert(list.total > 0);

  char * email = random_email(&arena);
  result = register_in_game(&db, list.games[0].id, email);
  assert(result == 0);

  RegistrationsList regs;
  result = read_confirmed_game_registrations(&db, &arena, list.games[0].id, &regs);
  assert(result == 0);
  assert(regs.total == 0);

  result = read_not_confirmed_registrations_over_time_limit(&db, &arena, &regs);
  assert(result == 0);
  assert(regs.total == 0);

  Game game;
  result = read_game(&db, list.games[0].id, &game);
  assert(result == 0);
  assert(strcmp(game.name, list.games[0].name) == 0);

  Registration registration;
  result = read_game_registration(&db, game.id, email, &registration);
  assert(result == 0);
  assert(registration.game_id == game.id);

  result = confirm_registration(&db, registration.uuid);
  assert(result == 0);

  result = read_confirmed_game_registrations(&db, &arena, list.games[0].id, &regs);
  assert(result == 0);
  assert(regs.total == 1);

  result = unregister_from_game(&db, regs.registrations[0].uuid);
  assert(result == 0);

  free_arena(&arena);
  close_db(&db);
  return 0;
}
