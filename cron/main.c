#include <stdio.h>
#include "../server/secrets.c"
#include "../server/database.c"

int main(){
  DB db;
  init_db(&db, DATABASE_CONNECTION_STRING);

  Arena arena;
  init_arena(&arena, 50 * MB);

  RegistrationsList list;
  int result = read_not_confirmed_registrations_over_time_limit(&db, &arena, &list);
  if(result == ERR_DB_DISCONNECTED){
    printf("Failed to run cron, database is down\n");
    return -1;
  }
  if(result == ERR_DB_QUERY_FAILED){
    printf("Failed to pick registration from database: Query failed to run\n");
    return -2;
  }

  for(int i=0; i < list.total; i++){
    printf("Removing %s registration from game %d\n", list.registrations[i].email, list.registrations[i].game_id);
    unregister_from_game(&db, list.registrations[i].uuid);
  }

  close_db(&db);
  return 0;
}
