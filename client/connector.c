#include <curl/curl.h>
#include "../common/allocator.c"
#include "parser.c"

#ifndef MB
  #define KB 1024
  #define MB (KB * 1024)
#endif

typedef struct Connector {
  char token[TOKEN_SIZE];
  Arena games_arena;
  Arena emails_arena;
  Arena send_email_arena;
} Connector;

typedef struct ConnectorResponse {
  char * buffer;
  size_t pos;
} ConnectorResponse;

typedef struct ConnectorRequest {
  CURL * curl;
  ConnectorResponse resp;
} ConnectorRequest;

size_t handle_response(void *ptr, size_t size, size_t nmemb, ConnectorResponse * response){
  memcpy(&response->buffer[response->pos], ptr, size*nmemb);
  response->pos = response->pos + size*nmemb;
  return size*nmemb;
}

ConnectorRequest * prepare_request(Arena * arena, char * endpoint){
  ConnectorRequest * request = alloc_in_arena(arena, sizeof(ConnectorRequest));

  curl_global_init(CURL_GLOBAL_ALL);
  request->curl = curl_easy_init();
  if(!request->curl){
    curl_global_cleanup();
    return NULL;
  }

  request->resp.buffer = alloc_in_arena(arena, EMAIL_BODY_SIZE);
  request->resp.pos = 0;

  curl_easy_setopt(request->curl, CURLOPT_URL, endpoint);
  curl_easy_setopt(request->curl, CURLOPT_WRITEFUNCTION, handle_response);
  curl_easy_setopt(request->curl, CURLOPT_WRITEDATA, &request->resp);
  return request;
}

CURLcode do_request(ConnectorRequest * request){
  CURLcode res = curl_easy_perform(request->curl);
  if(res != CURLE_OK){
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    return -1;
  }
  return res;
}

void request_cleanup(ConnectorRequest * request){
  curl_easy_cleanup(request->curl);
  curl_global_cleanup();
}

GamesList * get_games(Connector * conn){
  reset_arena(&conn->games_arena);
  char endpoint[300];
  snprintf(endpoint, 300, "%s/mailing_list/get_games", SERVER_URL);
  ConnectorRequest * request = prepare_request(&conn->games_arena, endpoint);

  char fields[TOKEN_SIZE + 50];
  snprintf(fields, TOKEN_SIZE + 50, "token=%s", conn->token);
  curl_easy_setopt(request->curl, CURLOPT_POSTFIELDS, fields);

  CURLcode result = do_request(request);
  GamesList * list = NULL;
  if(result == CURLE_OK){
    list = parse_get_games(request->resp.buffer, &conn->games_arena);
  }

  request_cleanup(request);
  return list;
}

EmailsList * get_game_emails(Connector * conn, int game_id){
  reset_arena(&conn->emails_arena);

  char endpoint[300];
  snprintf(endpoint, 300, "%s/mailing_list/get_emails", SERVER_URL);
  ConnectorRequest * request = prepare_request(&conn->emails_arena, endpoint);

  char fields[TOKEN_SIZE + INT_MAX_DIGITS + 100];
  snprintf(fields, TOKEN_SIZE + INT_MAX_DIGITS + 100, "token=%s&game_id=%d", conn->token, game_id);
  curl_easy_setopt(request->curl, CURLOPT_POSTFIELDS, fields);

  CURLcode result = do_request(request);
  EmailsList * list = NULL;
  if(result == CURLE_OK){
    list = parse_get_emails(request->resp.buffer, &conn->emails_arena);
  }

  request_cleanup(request);
  return list;
}

int send_email(Connector * conn, int game_id, char * subject, char * body){
  char endpoint[300];
  snprintf(endpoint, 300, "%s/mailing_list/send_email", SERVER_URL);
  ConnectorRequest * request = prepare_request(&conn->send_email_arena, endpoint);

  char * fields = alloc_in_arena(&conn->send_email_arena, 2 * EMAIL_BODY_SIZE);
  char * escaped_subject = curl_easy_escape(request->curl, subject, strlen(subject));
  if(!escaped_subject){
    printf("Failed to escape the mail subject: %s\n", subject);
    return -1;
  }
  char * escaped_body = curl_easy_escape(request->curl, body, strlen(body));
  if(!escaped_body){
    printf("Failed to escape the mail body: %s\n", body);
    curl_free(escaped_subject);
    return -1;
  }

  snprintf(
    fields,
    TOKEN_SIZE + INT_MAX_DIGITS + KB + EMAIL_BODY_SIZE + 255,
    "token=%s&game_id=%d&subject=%s&body=%s",
    conn->token, game_id, escaped_subject, escaped_body
  );
  curl_easy_setopt(request->curl, CURLOPT_POSTFIELDS, fields);

  CURLcode result = do_request(request);

  curl_free(escaped_subject);
  curl_free(escaped_body);
  request_cleanup(request);
  reset_arena(&conn->send_email_arena);
  return (int)result;
}

