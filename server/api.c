#include <string.h>
#include <stdbool.h>
#include <libhttp.h>
#include "database.c"
#include "email.c"
#include "../common/allocator.c"
#include "validators.c"
#include "formatter.c"
#include "time.c"
#include <pthread.h>

#define THREADS 5
#define ARENA_SPACE (5 * MB)

// Minified drmargarido_registration.html
#define REGISTRATION_EMAIL "<table style=text-align:center width=100%><tr><td width=33%><td width=34% style=padding-top:30px;padding-bottom:30px><img alt='DRMargarido Logo'src=https://smargarido.com/images/favicon32.png><h1>Thanks for your registration!</h1><h4>I will mainly send you news when I release games or do interesting projects.</h4><p class=small style=font-size:14px>You can expect this to be a low traffic list.<td width=33%></table>"

typedef struct Api {
  struct lh_ctx_t *context;
  DB * db;
  EmailDaemon * daemon;
  Pool arenas_pool;
  pthread_mutex_t pool_lock;
  char token[TOKEN_SIZE];
} Api;

Arena api_get_arena(Api * api){
  Arena arena = {0};

  pthread_mutex_lock(&api->pool_lock);
  void * buffer = alloc_in_pool(&api->arenas_pool);
  pthread_mutex_unlock(&api->pool_lock);

  init_arena_with_buffer(&arena, buffer, ARENA_SPACE);
  return arena;
}

// This free should only be used with arenas obtained using the api_get_arena call
void api_free_arena(Api * api, Arena * arena){
  pthread_mutex_lock(&api->pool_lock);
  free_in_pool(&api->arenas_pool, arena->buffer);
  pthread_mutex_unlock(&api->pool_lock);
}

int success_response(struct lh_ctx_t * context, struct lh_con_t * conn, char * message){
  httplib_printf(context, conn,
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: %ld\r\n"        // Always set Content-Length
    "\r\n"
    "%s",
    strlen(message), message
  );
  return 200;
}

int error_response(struct lh_ctx_t * context, struct lh_con_t * conn, char * message){
  httplib_printf(context, conn,
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: %ld\r\n"        // Always set Content-Length
    "\r\n"
    "%s",
    strlen(message), message
  );
  return 400;
}

#define ERROR_NO_MORE_MEMORY "Sorry for the incovenience, the server is with "\
  "some hiccups, please try again later. "
#define ERROR_DATABASE_IS_DOWN "Sorry for the incovenience, we are having some"\
  " problems connecting to the database, please try again later."
#define ERROR_INVALID_EMAIL "Email has an invalid format."
#define ERROR_INVALID_GAME_ID "Received an invalid game_id."
#define ERROR_GAME_DOESNT_EXIST "The received game doesn't exist."
#define ERROR_EMAIL_ALREADY_REGISTERED "This email is already registered in the game mailing list."
#define ERROR_REGISTRATION_FAILED "Failed to create the registration in the database"
#define SUCCESS_REGISTRATION "Email registered successfully, please check your email for a confirmation email."
#define SUCCESS_CONFIRM_REGISTRATION "Email Confirmed Successfully."
#define ERROR_UUID_DOESNT_EXIST "The received token doesn't exist."
#define ERROR_CONFIRMATION_FAILED "Failed to confirm the registration."

// IMPROVEMENT: Centralize error responses so we reduce code for reporting in the api endpoints
static int api_register(struct lh_ctx_t * context, struct lh_con_t * conn, void *data){
  char post_data[EMAIL_SIZE + INT_MAX_DIGITS + 256];
  char game_id[INT_MAX_DIGITS];
  char user_email[EMAIL_SIZE];
  int post_data_len;

  post_data_len = httplib_read(context, conn, post_data, sizeof(post_data));
  httplib_get_var(post_data, post_data_len, "email", user_email, EMAIL_SIZE);
  httplib_get_var(post_data, post_data_len, "game_id", game_id, INT_MAX_DIGITS);

  int id = atoi(game_id);
  if(id == 0){
    return error_response(context, conn, ERROR_INVALID_GAME_ID);
  }

  if(!is_email_valid(user_email)){
    printf("Received email with invalid format: %s\n", user_email);
    return error_response(context, conn, ERROR_INVALID_EMAIL);
  }

  Api * api = (Api *) data;

  Game game;
  int result = read_game(api->db, id, &game);
  if(result == ERR_DB_DISCONNECTED){
    return error_response(context, conn, ERROR_DATABASE_IS_DOWN);
  }
  if(result == ERR_ID_DOESNT_EXIST){
    printf("Received game_id that does not exist: %s\n", game_id);
    return error_response(context, conn, ERROR_GAME_DOESNT_EXIST);
  }

  result = register_in_game(api->db, id, user_email);
  if(result == ERR_DB_DISCONNECTED){
    return error_response(context, conn, ERROR_DATABASE_IS_DOWN);
  }
  if(result == ERR_DB_QUERY_FAILED){
    printf("Failed to register the email in the database, probably is already registered.\n");
    // Report success to not leak that we already have this email
    return success_response(context, conn, SUCCESS_REGISTRATION);
  }

  Registration registration;
  result = read_game_registration(api->db, id, user_email, &registration);
  if(result == ERR_DB_DISCONNECTED){
    return error_response(context, conn, ERROR_DATABASE_IS_DOWN);
  }

  Arena arena = api_get_arena(api);

  Email * prepared_email = alloc_in_arena(&arena, sizeof(Email));
  build_confirm_registration_email(prepared_email, api->daemon->config->username, user_email, registration.uuid, game.name);
  if(queue_email(api->daemon, prepared_email) != 0){
    printf("Failed sending the confirmation email to the new registered user '%s'\n", user_email);
  }

  api_free_arena(api, &arena);
  return success_response(context, conn, SUCCESS_REGISTRATION);
}

#define ERROR_NO_TOKEN "Token field is mandatory"
#define ERROR_INVALID_TOKEN "The received token is not valid."
#define ERROR_NO_REGISTRATION "The received token is not associated with any registration."
#define ERROR_FAILED_TO_REMOVE "Failed trying to remove the registration."
#define UNREGISTER_SUCCESS "Unregistered from the mailing list."

static int api_confirm_registration(struct lh_ctx_t * context, struct lh_con_t * conn, void *data){
  const struct lh_rqi_t *request_info = httplib_get_request_info(conn);
  if(!request_info->query_string){
    return error_response(context, conn, ERROR_NO_TOKEN);
  }

  char token[UUID_SIZE] = "";
  int len = strlen(request_info->query_string) + 1;
  httplib_get_var(request_info->query_string, len, "token", token, UUID_SIZE);

  if(strlen(token) != UUID_SIZE - 1){
    return error_response(context, conn, ERROR_INVALID_TOKEN);
  }

  Api * api = (Api *) data;

  Registration registration;
  int result = read_game_registration_by_uuid(api->db, token, &registration);
  if(result == ERR_DB_DISCONNECTED){
    return error_response(context, conn, ERROR_DATABASE_IS_DOWN);
  }
  if(result == ERR_UUID_DOESNT_EXIST){
    printf("Received a uuid that does not exist: %s\n", token);
    return error_response(context, conn, ERROR_UUID_DOESNT_EXIST);
  }

  if(registration.email_confirmed){
    printf("Ignoring request, registration was already confirmed: %s\n", token);
    return success_response(context, conn, SUCCESS_CONFIRM_REGISTRATION);
  }

  result = confirm_registration(api->db, token);
  if(result == ERR_DB_DISCONNECTED){
    return error_response(context, conn, ERROR_DATABASE_IS_DOWN);
  }
  if(result == ERR_DB_QUERY_FAILED){
    printf("Failed to confirm the registration for the uuid: %s\n", token);
    return error_response(context, conn, ERROR_CONFIRMATION_FAILED);
  }

  Game game;
  result = read_game(api->db, registration.game_id, &game);
  if(result == ERR_DB_DISCONNECTED){
    return error_response(context, conn, ERROR_DATABASE_IS_DOWN);
  }

  char title[TITLE_SIZE] = "";
  snprintf(title, TITLE_SIZE, "Welcome to the %s Mailing List", game.name);

  Arena arena = api_get_arena(api);

  char * body = alloc_in_arena(&arena, EMAIL_BODY_SIZE);
  strncpy(body, REGISTRATION_EMAIL, EMAIL_BODY_SIZE);

  Email * prepared_email = alloc_in_arena(&arena, sizeof(Email));
  build_email(prepared_email, api->daemon->config->username, registration.email, title, body, token);
  if(queue_email(api->daemon, prepared_email) != 0){
    printf("Failed sending email to the new registered user\n");
  }

  api_free_arena(api, &arena);
  return success_response(context, conn, SUCCESS_CONFIRM_REGISTRATION);
}

static int api_unregister(struct lh_ctx_t * context, struct lh_con_t * conn, void *data){
  Api * api = (Api *) data;

  const struct lh_rqi_t *request_info = httplib_get_request_info(conn);
  if(!request_info->query_string){
    return error_response(context, conn, ERROR_NO_TOKEN);
  }

  char token[UUID_SIZE] = "";
  int len = strlen(request_info->query_string) + 1;
  httplib_get_var(request_info->query_string, len, "token", token, UUID_SIZE);

  if(strlen(token) != UUID_SIZE - 1){
    return error_response(context, conn, ERROR_INVALID_TOKEN);
  }

  int result = unregister_from_game(api->db, token);
  if(result != 0){
    if(result == ERR_DB_DISCONNECTED){
      return error_response(context, conn, ERROR_DATABASE_IS_DOWN);
    }
    else if(result == 1) {
      return error_response(context, conn, ERROR_NO_REGISTRATION);
    }
    else {
      printf("Failed to remove the registration\n");
      return error_response(context, conn, ERROR_FAILED_TO_REMOVE);
    }
  }

  return success_response(context, conn, UNREGISTER_SUCCESS);
}

// TODO: Add tests for the new api endpoints
// TODO: Make the client_api work directly in binary to simplify communication,
// remove need for json encoding / parsing and also have more control over
// memory usage.

#define ERROR_NO_AUTH_TOKEN "A valid token field is mandatory to ask for data"

static int client_api_get_games(struct lh_ctx_t * context, struct lh_con_t * conn, void *data){
  char post_data[TOKEN_SIZE + 256];
  char token[TOKEN_SIZE];
  int post_data_len;

  post_data_len = httplib_read(context, conn, post_data, sizeof(post_data));
  httplib_get_var(post_data, post_data_len, "token", token, TOKEN_SIZE);

  Api * api = (Api *) data;
  if(!token || strcmp(token, api->token) != 0){
    printf("Missing mandatory token!\n");
    return error_response(context, conn, ERROR_NO_TOKEN);
  }

  Arena arena = api_get_arena(api);
  GamesList list;
  int result = read_games(api->db, &arena, &list);
  if(result == ERR_DB_DISCONNECTED){
    api_free_arena(api, &arena);
    return error_response(context, conn, ERROR_DATABASE_IS_DOWN);;
  }

  char * json = games_list_to_json(&list, &arena);
  success_response(context, conn, json);
  api_free_arena(api, &arena);
  return 200;
}

static int client_api_get_emails(struct lh_ctx_t * context, struct lh_con_t * conn, void *data){
  char post_data[TOKEN_SIZE + INT_MAX_DIGITS + 256];
  char token[TOKEN_SIZE];
  char game_id_text[INT_MAX_DIGITS];
  int post_data_len;

  post_data_len = httplib_read(context, conn, post_data, sizeof(post_data));
  httplib_get_var(post_data, post_data_len, "token", token, TOKEN_SIZE);
  httplib_get_var(post_data, post_data_len, "game_id", game_id_text, INT_MAX_DIGITS);

  Api * api = (Api *) data;
  if(!token || strcmp(token, api->token) != 0){
    return error_response(context, conn, ERROR_NO_TOKEN);
  }

  int game_id = atoi(game_id_text);
  if(game_id == 0){
    return error_response(context, conn, ERROR_INVALID_GAME_ID);
  }

  Game game;
  int result = read_game(api->db, game_id, &game);
  if(result == ERR_DB_DISCONNECTED){
    return error_response(context, conn, ERROR_DATABASE_IS_DOWN);
  }
  if(result == ERR_ID_DOESNT_EXIST){
    printf("Received game_id that does not exist: %d\n", game_id);
    return error_response(context, conn, ERROR_GAME_DOESNT_EXIST);
  }

  Arena arena = api_get_arena(api);
  RegistrationsList list;
  result = read_confirmed_game_registrations(api->db, &arena, game_id, &list);
  if(result == ERR_DB_DISCONNECTED){
    api_free_arena(api, &arena);
    return error_response(context, conn, ERROR_DATABASE_IS_DOWN);
  }

  char * json = emails_list_to_json(&list, &arena);
  success_response(context, conn, json);
  api_free_arena(api, &arena);
  return 200;
}

#define ERROR_EMPTY_EMAIL_FIELDS "Some of the email fields is empty."
#define SEND_EMAIL_SUCCESS "Email Submitted Successfully."

static int client_api_send_email(struct lh_ctx_t * context, struct lh_con_t * conn, void *data){
  Api * api = (Api *) data;
  Arena arena = api_get_arena(api);

  char * post_data = alloc_in_arena(&arena, TOKEN_SIZE + INT_MAX_DIGITS + KB + EMAIL_BODY_SIZE + 256);
  char token[TOKEN_SIZE];
  char game_id_text[INT_MAX_DIGITS];
  char subject[KB];
  char * body = alloc_in_arena(&arena, EMAIL_BODY_SIZE);
  int post_data_len;

  post_data_len = httplib_read(context, conn, post_data, TOKEN_SIZE + INT_MAX_DIGITS + KB + EMAIL_BODY_SIZE + 256);
  httplib_get_var(post_data, post_data_len, "token", token, TOKEN_SIZE);
  httplib_get_var(post_data, post_data_len, "game_id", game_id_text, INT_MAX_DIGITS);
  httplib_get_var(post_data, post_data_len, "subject", subject, KB);
  httplib_get_var(post_data, post_data_len, "body", body, EMAIL_BODY_SIZE);

  if(!token || strcmp(token, api->token) != 0){
    printf("Missing mandatory token!\n");
    api_free_arena(api, &arena);
    return error_response(context, conn, ERROR_NO_TOKEN);
  }

  int game_id = atoi(game_id_text);
  if(game_id == 0){
    api_free_arena(api, &arena);
    return error_response(context, conn, ERROR_INVALID_GAME_ID);
  }

  Game game;
  int result = read_game(api->db, game_id, &game);
  if(result == ERR_DB_DISCONNECTED){
    api_free_arena(api, &arena);
    return error_response(context, conn, ERROR_DATABASE_IS_DOWN);
  }
  if(result == ERR_ID_DOESNT_EXIST){
    printf("Received game_id that does not exist: %d\n", game_id);
    api_free_arena(api, &arena);
    return error_response(context, conn, ERROR_GAME_DOESNT_EXIST);
  }

  if(!subject || !body){
    printf("Empty email fields!\n");
    api_free_arena(api, &arena);
    return error_response(context, conn, ERROR_EMPTY_EMAIL_FIELDS);
  }

  queue_group_email(api->daemon, subject, body, game_id);
  api_free_arena(api, &arena);
  return success_response(context, conn, SEND_EMAIL_SUCCESS);
}

void log_request(const struct lh_con_t * conn, int result, long double duration){
  const struct lh_rqi_t * info = httplib_get_request_info(conn);
  #ifdef RELEASE
    const char * host = httplib_get_header(conn, "X-Real-IP");
  #else
    const char * host = httplib_get_header(conn, "Host");
  #endif

  printf("%s - %d [%s] %LFms\n", info->local_uri, result, host, duration);
  fflush(stdout);
}

static int begin_request_handler(struct lh_ctx_t * context, struct lh_con_t * conn){
  start_timer(httplib_pthread_self());
  return 0;
}

static void end_request_handler(struct lh_ctx_t * context, const struct lh_con_t * conn, int reply_status_code){
  long double duration = end_timer(httplib_pthread_self());
  log_request(conn, reply_status_code, duration);
}

int init_api(Api * api, DB * db, EmailDaemon * daemon, char * token){
  struct lh_clb_t callbacks;
  memset(&callbacks, 0, sizeof(callbacks));

  callbacks.begin_request = begin_request_handler;
  callbacks.end_request = end_request_handler;
  const struct lh_opt_t ports = {"listening_ports", "8080"};
  char threads_text[3];
  snprintf(threads_text, 3, "%d", THREADS);
  const struct lh_opt_t threads = {"num_threads", threads_text};
  const struct lh_opt_t options[3] = {ports, threads, NULL};
  api->context = httplib_start(&callbacks, NULL, options);
  api->db = db;
  api->daemon = daemon;
  strncpy(api->token, token, TOKEN_SIZE);

  int result = init_pool(&api->arenas_pool, ARENA_SPACE, THREADS);
  if(result != 0){
    printf("Failed to initialize arenas pool in the api\n");
    return -1;
  }

  if(pthread_mutex_init(&api->pool_lock, NULL) != 0){
    printf("Failed to initialize the api pool mutex\n");
    return -1;
  }

  httplib_set_request_handler(api->context, "/mailing_list/register", api_register, (void *) api);
  httplib_set_request_handler(api->context, "/mailing_list/confirm_registration", api_confirm_registration, (void *) api);
  httplib_set_request_handler(api->context, "/mailing_list/unregister", api_unregister, (void *) api);
  httplib_set_request_handler(api->context, "/mailing_list/get_games", client_api_get_games, (void *) api);
  httplib_set_request_handler(api->context, "/mailing_list/get_emails", client_api_get_emails, (void *) api);
  httplib_set_request_handler(api->context, "/mailing_list/send_email", client_api_send_email, (void *) api);
}

int shutdown_api(Api * api){
  httplib_stop(api->context);
  pthread_mutex_destroy(&api->pool_lock);
  free_pool(&api->arenas_pool);
}
