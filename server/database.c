#ifndef DATABASE_INCLUDED
#define DATABASE_INCLUDED

#include "../common/structures.c"
#include "../common/allocator.c"
#include <libpq-fe.h>

#define ERR_DB_DISCONNECTED   -11
#define ERR_ID_DOESNT_EXIST   -12
#define ERR_DB_QUERY_FAILED   -13
#define ERR_UUID_DOESNT_EXIST -14

typedef struct DB {
  PGconn *conn;
  PGresult *result;
} DB;

// IMPROVEMENT: Talk to postgres in binary instead of text to prevent the need
// for some data conversion.

int init_db(DB * db, const char * conn_string){
  /* Make a connection to the database */
  db->conn = PQconnectdb(conn_string);

  /* Check to see that the backend connection was successfully made */
  if (PQstatus(db->conn) != CONNECTION_OK){
    fprintf(stderr, "Connection to database failed: %s", PQerrorMessage(db->conn));
    return -1;
  }

  return 0;
}

int check_db(DB * db){
  if (PQstatus(db->conn) == CONNECTION_OK){
    return 0;
  }

  // Try to reconnect
  fprintf(stderr, "Connection to database is borked: %s", PQerrorMessage(db->conn));
  PQreset(db->conn);
  if (PQstatus(db->conn) == CONNECTION_OK){
    return 0;
  }

  // Connection is broken
  return -1;
}

int create_game(DB *db, char name[GAME_NAME_SIZE]){
  if(check_db(db) != 0){
    printf("Connection to database is not working!\n");
    return ERR_DB_DISCONNECTED;
  }

  const char * query = "INSERT INTO game(name) VALUES ($1)";
  const char * values[1] = {name};
  int lengths[1] = {GAME_NAME_SIZE};
  int formats[1] = {0};

  db->result = PQexecParams(db->conn, query, 1, NULL, values, lengths, formats, 0);
  if (PQresultStatus(db->result) != PGRES_COMMAND_OK){
    fprintf(stderr, "Failed Creating Game %s\n", PQerrorMessage(db->conn));
    PQclear(db->result);
    return ERR_DB_QUERY_FAILED;
  }

  PQclear(db->result);
  return 0;
}

int read_game(DB *db, int game_id, Game * game_out){
  if(check_db(db) != 0){
    printf("Connection to database is not working!\n");
    return ERR_DB_DISCONNECTED;
  }

  const char * query = "SELECT name FROM game WHERE id=$1";
  char game_id_text[INT_MAX_DIGITS];
  snprintf(game_id_text, INT_MAX_DIGITS, "%d", game_id);
  const char * values[1] = {game_id_text};
  int lengths[1] = {INT_MAX_DIGITS};
  int formats[1] = {0};

  db->result = PQexecParams(db->conn, query, 1, NULL, values, lengths, formats, 0);
  if (PQresultStatus(db->result) != PGRES_TUPLES_OK){
    fprintf(stderr, "Error checking if a game exists %s\n", PQerrorMessage(db->conn));
    PQclear(db->result);
    return ERR_DB_QUERY_FAILED;
  }

  int total_rows = PQntuples(db->result);
  if(total_rows == 0){
    PQclear(db->result);
    return ERR_ID_DOESNT_EXIST;
  }

  game_out->id = game_id;
  strncpy(game_out->name, PQgetvalue(db->result, 0, 0), GAME_NAME_SIZE);
  PQclear(db->result);
  return 0;
}

int read_games(DB *db, Arena *arena, GamesList * list_out){
  if(check_db(db) != 0){
    printf("Connection to database is not working!\n");
    return ERR_DB_DISCONNECTED;
  }

  db->result = PQexec(db->conn, "SELECT id, name FROM game");
  if (PQresultStatus(db->result) != PGRES_TUPLES_OK){
    fprintf(stderr, "Failed Reading Games %s\n", PQerrorMessage(db->conn));
    PQclear(db->result);
    return ERR_DB_QUERY_FAILED;
  }

  int total_rows = PQntuples(db->result);
  list_out->games = alloc_in_arena(arena, sizeof(Game) * total_rows);
  list_out->total = total_rows;
  for (int i = 0; i < total_rows; i++){
    int id = atoi(PQgetvalue(db->result, i, 0));
    list_out->games[i].id = id;
    strncpy(list_out->games[i].name, PQgetvalue(db->result, i, 1), GAME_NAME_SIZE);
  }

  PQclear(db->result);
  return 0;
}

int register_in_game(DB *db, int game_id, char email[EMAIL_SIZE]){
  if(check_db(db) != 0){
    printf("Connection to database is not working!\n");
    return ERR_DB_DISCONNECTED;
  }

  const char * query = "INSERT INTO registration(game_id, email) VALUES ($1, LOWER($2))";
  char game_id_text[INT_MAX_DIGITS];
  snprintf(game_id_text, INT_MAX_DIGITS, "%d", game_id);
  const char * values[2] = {game_id_text, email};
  int lengths[2] = {INT_MAX_DIGITS, EMAIL_SIZE};
  int formats[2] = {0, 0};

  db->result = PQexecParams(db->conn, query, 2, NULL, values, lengths, formats, 0);
  if (PQresultStatus(db->result) != PGRES_COMMAND_OK){
    fprintf(stderr, "Failed Registration in List %s\n", PQerrorMessage(db->conn));
    PQclear(db->result);
    return ERR_DB_QUERY_FAILED;
  }

  PQclear(db->result);
  return 0;
}

int unregister_from_game(DB *db, char uuid[UUID_SIZE]){
  if(check_db(db) != 0){
    printf("Connection to database is not working!\n");
    return ERR_DB_DISCONNECTED;
  }

  const char * query = "DELETE FROM registration WHERE unregister_token=$1 RETURNING id";
  const char * values[1] = {uuid};
  int lengths[1] = {UUID_SIZE};
  int formats[1] = {0};
  db->result = PQexecParams(db->conn, query, 1, NULL, values, lengths, formats, 0);

  if (PQresultStatus(db->result) != PGRES_TUPLES_OK){
    fprintf(stderr, "Failed Deleting Registration %s\n", PQerrorMessage(db->conn));
    PQclear(db->result);
    return ERR_DB_QUERY_FAILED;
  }

  int total_deleted = PQntuples(db->result);
  PQclear(db->result);
  if(total_deleted == 0){
    printf("Delete ignored, there is no registration with the uuid - %s!\n", uuid);
    return 1;
  }

  return 0;
}

int read_game_registration(DB *db, int game_id, char email[EMAIL_SIZE], Registration * registration_out){
  if(check_db(db) != 0){
    printf("Connection to database is not working!\n");
    return ERR_DB_DISCONNECTED;
  }

  const char * query = "SELECT unregister_token, email_confirmed FROM registration WHERE game_id=$1 AND email=$2";
  char game_id_text[INT_MAX_DIGITS];
  snprintf(game_id_text, INT_MAX_DIGITS, "%d", game_id);
  const char * values[2] = {game_id_text, email};
  int lengths[2] = {INT_MAX_DIGITS, EMAIL_SIZE};
  int formats[2] = {0, 0};

  db->result = PQexecParams(db->conn, query, 2, NULL, values, lengths, formats, 0);
  if (PQresultStatus(db->result) != PGRES_TUPLES_OK){
    fprintf(stderr, "Failed Reading Registrations %s\n", PQerrorMessage(db->conn));
    PQclear(db->result);
    return ERR_DB_QUERY_FAILED;
  }

  int total_rows = PQntuples(db->result);
  if(total_rows == 0){
    fprintf(stderr, "No registration found\n");
    PQclear(db->result);
    return ERR_ID_DOESNT_EXIST;
  }

  registration_out->game_id = game_id;
  strncpy(registration_out->email, email, EMAIL_SIZE);
  strncpy(registration_out->uuid, PQgetvalue(db->result, 0, 0), UUID_SIZE);
  if(PQgetvalue(db->result, 0, 1)[0] == 't'){
    registration_out->email_confirmed = true;
  } else {
    registration_out->email_confirmed = false;
  }
  PQclear(db->result);
  return 0;
}

int read_game_registration_by_uuid(DB *db, char uuid[UUID_SIZE], Registration * registration_out){
  if(check_db(db) != 0){
    printf("Connection to database is not working!\n");
    return ERR_DB_DISCONNECTED;
  }

const char * query = "SELECT game_id, email, email_confirmed FROM registration WHERE unregister_token=$1";
  const char * values[1] = {uuid};
  int lengths[1] = {UUID_SIZE};
  int formats[1] = {0};

  db->result = PQexecParams(db->conn, query, 1, NULL, values, lengths, formats, 0);
  if (PQresultStatus(db->result) != PGRES_TUPLES_OK){
    fprintf(stderr, "Failed reading registration %s\n", PQerrorMessage(db->conn));
    PQclear(db->result);
    return ERR_DB_QUERY_FAILED;
  }

  int total_rows = PQntuples(db->result);
  if(total_rows == 0){
    fprintf(stderr, "No registration found\n");
    PQclear(db->result);
    return ERR_UUID_DOESNT_EXIST;
  }

  registration_out->game_id = atoi(PQgetvalue(db->result, 0, 0));
  strncpy(registration_out->email, PQgetvalue(db->result, 0, 1), EMAIL_SIZE);
  strncpy(registration_out->uuid, uuid, UUID_SIZE);
  if(PQgetvalue(db->result, 0, 2)[0] == 't'){
    registration_out->email_confirmed = true;
  } else {
    registration_out->email_confirmed = false;
  }
  PQclear(db->result);
  return 0;
}

int confirm_registration(DB *db, char uuid[UUID_SIZE]){
  if(check_db(db) != 0){
    printf("Connection to database is not working!\n");
    return ERR_DB_DISCONNECTED;
  }

  const char * query = "UPDATE registration SET email_confirmed = true WHERE unregister_token=$1";
  const char * values[1] = {uuid};
  int lengths[1] = {UUID_SIZE};
  int formats[1] = {0};

  db->result = PQexecParams(db->conn, query, 1, NULL, values, lengths, formats, 0);
  if (PQresultStatus(db->result) != PGRES_COMMAND_OK){
    fprintf(stderr, "Failed to confirm the registration %s\n", PQerrorMessage(db->conn));
    PQclear(db->result);
    return ERR_DB_QUERY_FAILED;
  }

  PQclear(db->result);
  return 0;
}

/*TODO: We can add support for pagination so we can make the memory usage fixed*/
int read_confirmed_game_registrations(DB *db, Arena *arena, int game_id, RegistrationsList * list_out){
  if(check_db(db) != 0){
    printf("Connection to database is not working!\n");
    return ERR_DB_DISCONNECTED;
  }

  const char * query = "SELECT game_id, email, unregister_token FROM registration WHERE game_id=$1 and email_confirmed=true";
  char game_id_text[INT_MAX_DIGITS];
  snprintf(game_id_text, INT_MAX_DIGITS, "%d", game_id);
  const char * values[1] = {game_id_text};
  int lengths[1] = {INT_MAX_DIGITS};
  int formats[1] = {0};

  db->result = PQexecParams(db->conn, query, 1, NULL, values, lengths, formats, 0);
  if (PQresultStatus(db->result) != PGRES_TUPLES_OK){
    fprintf(stderr, "Failed Reading Registrations %s\n", PQerrorMessage(db->conn));
    PQclear(db->result);
    return ERR_DB_QUERY_FAILED;
  }

  int total_rows = PQntuples(db->result);
  list_out->registrations = alloc_in_arena(arena, sizeof(Registration) * total_rows);
  list_out->total = total_rows;
  for(int i=0; i < total_rows; i++){
    list_out->registrations[i].game_id = atoi(PQgetvalue(db->result, i, 0));
    strncpy(list_out->registrations[i].email, PQgetvalue(db->result, i, 1), EMAIL_SIZE);
    strncpy(list_out->registrations[i].uuid, PQgetvalue(db->result, i, 2), UUID_SIZE);
    list_out->registrations[i].email_confirmed = true;
  }

  PQclear(db->result);
  return 0;
}

  int read_not_confirmed_registrations_over_time_limit(DB *db, Arena *arena, RegistrationsList * list_out){
  if(check_db(db) != 0){
    printf("Connection to database is not working!\n");
    return ERR_DB_DISCONNECTED;
  }

  const char * query = "SELECT game_id, email, unregister_token FROM registration WHERE email_confirmed = false AND (NOW() - INTERVAL '1 day') > creation_date";
  db->result = PQexec(db->conn, query);
  if (PQresultStatus(db->result) != PGRES_TUPLES_OK){
    fprintf(stderr, "Failed Reading All Registrations %s\n", PQerrorMessage(db->conn));
    PQclear(db->result);
    return ERR_DB_QUERY_FAILED;
  }

  int total_rows = PQntuples(db->result);
  list_out->registrations = alloc_in_arena(arena, sizeof(Registration) * total_rows);
  list_out->total = total_rows;
  for(int i=0; i < total_rows; i++){
    list_out->registrations[i].game_id = atoi(PQgetvalue(db->result, i, 0));
    strncpy(list_out->registrations[i].email, PQgetvalue(db->result, i, 1), EMAIL_SIZE);
    strncpy(list_out->registrations[i].uuid, PQgetvalue(db->result, i, 2), UUID_SIZE);
    list_out->registrations[i].email_confirmed = false;
  }

  PQclear(db->result);
  return 0;
}


void close_db(DB * db){
  PQfinish(db->conn);
}

#endif
