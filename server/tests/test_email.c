#include <assert.h>
#include <unistd.h>
#include "../../common/allocator.c"
#include "../email.c"
#include "../database.c"

#define MILLISECOND 1000
#define SECOND (1000 * MILLISECOND)

int main(int argc, char ** argv){
  DB db;
  const char *conn_string = DATABASE_CONNECTION_STRING;
  init_db(&db, conn_string);

  Arena arena;
  init_arena(&arena, 1 * MB);
  Email * email = alloc_in_arena(&arena, sizeof(Email));
  build_email(email, EMAIL_USERNAME, EMAIL_USERNAME, "Testing sending emails", "Hey how am I?", "123-123123-123132-123");

  EmailConfig config = {EMAIL_USERNAME, EMAIL_PASSWORD};
  assert(send_email(&config, email) == 0);

  EmailDaemon daemon;
  assert(init_email_daemon(&daemon, &config, 10, 0, &db) == 0);

  queue_email(&daemon, email);
  #ifdef NO_EMAIL
    usleep(2 * MILLISECOND);
  #else
    usleep(2 * SECOND);
  #endif
  printf("Sent emails count: %ld\n", daemon.sent_emails);
  assert(daemon.sent_emails == 1);

  shutdown_email_daemon(&daemon);
  free_arena(&arena);
  close_db(&db);
  return 0;
}
