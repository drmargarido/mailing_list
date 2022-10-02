#include "secrets.c"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include "../common/allocator.c"
#include "../common/structures.c"
#include "database.c"
#include "email.c"
#include "api.c"

#ifndef KB
  #define KB 1024
  #define MB (1024 * KB)
#endif

#define MILLISECOND 1000

#define QUEUE_CAPACITY 500
#define TIME_BETWEEN_EMAILS 1

bool is_running = true;
void termination_handler (int signum){
  is_running = false;
}

int main(int argc, char**argv){
  // Register signal handling for gracefull shutdown
  struct sigaction action;
  action.sa_handler = termination_handler;
  action.sa_flags = SA_SIGINFO;
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);

  const char *conn_string = DATABASE_CONNECTION_STRING;

  DB db = {};
  int result = init_db(&db, conn_string);
  if(result != 0){
    return -1;
  }

  EmailConfig config = {EMAIL_USERNAME, EMAIL_PASSWORD};
  EmailDaemon daemon;
  init_email_daemon(&daemon, &config, QUEUE_CAPACITY, TIME_BETWEEN_EMAILS, &db);

  Api api;
  init_api(&api, &db, &daemon, API_TOKEN);

  printf("Server is running!\n");
  while(is_running){
    usleep(100 * MILLISECOND);
  }
  printf("Server is stopping!\n");

  shutdown_api(&api);
  shutdown_email_daemon(&daemon);
  close_db(&db);
  return 0;
}
