#include <stdlib.h>
#include <assert.h>
#include <curl/curl.h>
#include "../api.c"

static int test_api_register(CURL * curl, char email[EMAIL_SIZE]){
  char url[300] = "";
  snprintf(url, 300, "%s/mailing_list/register", SERVER_URL) ;
  curl_easy_setopt(curl, CURLOPT_URL, url);

  char fields[400] = "";
  snprintf(fields, 400, "email=%s&game_id=1", email);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fields);

  CURLcode res = curl_easy_perform(curl);
  if(res != CURLE_OK){
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    return -1;
  }
  int http_code;
  curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
  return http_code;
}

static int test_api_confirm_registration(CURL * curl, char token[UUID_SIZE]){
  char url[300] = "";
  snprintf(url, 300, "%s/mailing_list/confirm_registration?token=%s", SERVER_URL, token);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  CURLcode res = curl_easy_perform(curl);
  if(res != CURLE_OK){
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    return -1;
  }
  int http_code;
  curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
  return http_code;
}

static int test_api_unregister(CURL * curl, char token[UUID_SIZE]){
  char url[300] = "";
  snprintf(url, 300, "%s/mailing_list/unregister?token=%s", SERVER_URL, token) ;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  CURLcode res = curl_easy_perform(curl);
  if(res != CURLE_OK){
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    return -1;
  }
  int http_code;
  curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
  return http_code;
}

int main(int argc, char ** argv){
  DB db;
  const char *conn_string = DATABASE_CONNECTION_STRING;
  init_db(&db, conn_string);

  EmailConfig config = {EMAIL_USERNAME, EMAIL_PASSWORD};
  EmailDaemon daemon;
  init_email_daemon(&daemon, &config, 10, 0, &db);

  Api api;
  init_api(&api, &db, &daemon, "");

  CURL *curl;

  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  if(!curl){
    curl_global_cleanup();
    shutdown_api(&api);
    close_db(&db);
    return -1;
  }

  assert(test_api_register(curl, "margarido@mail.com") == 200);
  assert(test_api_register(curl, "mysite.ourearth.com") == 400);
  assert(test_api_register(curl, "mysite@.com.my") == 400);
  assert(test_api_register(curl, "@you.me.net") == 400);
  assert(test_api_register(curl, "mysite@.org.org") == 400);
  assert(test_api_register(curl, ".mysite@mysite.org") == 400);
  assert(test_api_register(curl, "mysite()*@gmail.com") == 400);
  assert(test_api_register(curl, "mysite..1234@yahoo.com") == 400);

  Registration r;
  int result = read_game_registration(&db, 1, "margarido@mail.com", &r);
  assert(result == 0);
  assert(test_api_confirm_registration(curl, "randomgarbage") == 400);
  assert(test_api_confirm_registration(curl, r.uuid) == 200);

  assert(test_api_unregister(curl, "randomgarbage") == 400);
  assert(test_api_unregister(curl, r.uuid) == 200);
  assert(test_api_unregister(curl, r.uuid) == 400);

  curl_easy_cleanup(curl);
  curl_global_cleanup();
  shutdown_api(&api);
  shutdown_email_daemon(&daemon);
  close_db(&db);
  return 0;
}
