#include "secrets.c"
#include "gui.c"

int main (int argc, char **argv){
  Connector conn;
  snprintf(conn.token, TOKEN_SIZE, API_TOKEN);
  init_arena(&conn.games_arena, 100 * MB);
  init_arena(&conn.emails_arena, 100 * MB);
  init_arena(&conn.send_email_arena, 2 * MB);

  start_gui(&conn);

  free_arena(&conn.games_arena);
  free_arena(&conn.emails_arena);
  free_arena(&conn.send_email_arena);
}
