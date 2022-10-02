#ifndef STRUCTURES_INCLUDED
#define STRUCTURES_INCLUDED

// Max digits is 10 + 1 for the minus sign and one more for the \0 if needed
#define INT_MAX_DIGITS 12

#define KB 1024
#define MB (KB * 1024)

#define GAME_NAME_SIZE 200
#define EMAIL_SIZE 254
#define UUID_SIZE 37

#define TOKEN_SIZE 50
#define EMAIL_BODY_SIZE (100 * KB)

#include <stdbool.h>

typedef struct Game {
  int id;
  char name[GAME_NAME_SIZE];
} Game;

typedef struct GamesList {
  Game * games;
  int    total;
} GamesList;

typedef struct Registration {
  int game_id;
  char email[EMAIL_SIZE];
  char uuid[UUID_SIZE];
  bool email_confirmed;
} Registration;

typedef struct RegistrationsList {
  Registration * registrations;
  int total;
} RegistrationsList;

typedef struct EmailsList {
  int total;
  char * emails;
} EmailsList;

void display_gamelist(GamesList * list){
  for(int i=0; i < list->total; i++){
    printf("Game: %d | %s\n", list->games[i].id, list->games[i].name);
  }
}

void display_registrationslist(RegistrationsList * list){
  for(int i=0; i < list->total; i++){
    printf(
      "Registration: %d | %s | %s\n",
      list->registrations[i].game_id, list->registrations[i].email, list->registrations[i].uuid
    );
  }
}

#endif
