#ifndef EMAIL_INCLUDED
#define EMAIL_INCLUDED

#include "secrets.c"
#include <curl/curl.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "../common/structures.c"
#include "utils.c"
#include "database.c"

#ifndef KB
  #define KB 1024
  #define MB (1024 * KB)
#endif
#define TITLE_SIZE KB

typedef struct EmailConfig {
  char username[EMAIL_SIZE];
  char password[30];
} EmailConfig;

typedef struct Email {
  char from[EMAIL_SIZE];
  char to[EMAIL_SIZE];
  char subject[KB];
  char body[EMAIL_BODY_SIZE];
  size_t progress;
} Email;

static size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userp){
  if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
    return 0;
  }

  Email * email = (Email *)userp;
  const char *data = &(email->body[email->progress]);
  if(data) {
    size_t len_to_read = min(strlen(data), size * nmemb);
    memcpy(ptr, data, len_to_read);
    email->progress += len_to_read;
    return len_to_read;
  }

  return 0;
}

#define UNREGISTER_TEMPLATE "" \
  "</br><hr width='50%%' style='text-align: center;'>" \
  "<p style='text-align: center; font-size: 10px; margin-bottom: 15px;'>" \
  "If you don't want to receive " \
  "more emails from this mailing list you can unsubscribe with the following " \
  "<a href='%s/mailing_list/unregister?token=%s'>link</a></p>"

#define CONFIRM_REGISTRATION_TEMPLATE "<table style=text-align:center width=100%%><tr><td width=33%%><td width=34%% style=padding-top:30px;padding-bottom:30px><img alt='DRMargarido Logo'src=https://smargarido.com/images/favicon32.png><h1>Thanks for registering to the mailing list!</h1><h4>Please confirm your registration with the <a href='%s/mailing_list/confirm_registration?token=%s'>link</a>.</h4><p class=small style=font-size:14px>If you didn't register to this mailing list ignore this email, your email will be deleted from our server in 24h.<td width=33%%></table>"
#define CONFIRM_REGISTRATION_TITLE_TEMPLATE "Confirm your registration in the %s mailing list"

void get_current_time(char str[50]){
  time_t now;
  time(&now);
  struct tm * current_time = localtime(&now);
  strftime(str, 50, "%a, %d %b %Y %H:%M:%S +0000", current_time);
}

int build_confirm_registration_email(Email * email, char from[EMAIL_SIZE], char to[EMAIL_SIZE], char uuid[UUID_SIZE], char game_name[GAME_NAME_SIZE]){
  char title[KB] = "";
  snprintf(title, KB, CONFIRM_REGISTRATION_TITLE_TEMPLATE, game_name);

  strncpy(email->from, from, EMAIL_SIZE);
  strncpy(email->to, to, EMAIL_SIZE);
  strncpy(email->subject, title, KB);

  char confirm_registration_text[KB] = "";
  snprintf(confirm_registration_text, KB, CONFIRM_REGISTRATION_TEMPLATE, SERVER_URL, uuid);
  char date_str[50];
  get_current_time(date_str);

  snprintf(
    email->body, EMAIL_BODY_SIZE,
    "Date: %s\r\n" \
    "To: %s\r\n" \
    "From: %s\r\n" \
    "Subject: %s\r\n" \
    "Content-Type: text/html; charset=\"latin-1\"\n" \
    "Mime-version: 1.0\n" \
    "\r\n" /* empty line to divide headers from body, see RFC5322 */ \
    "%s\r\n",
    date_str, to, from, title, confirm_registration_text
  );
  return 0;
}

int build_email(Email * email, char from[EMAIL_SIZE], char to[EMAIL_SIZE], char title[KB], char content[EMAIL_BODY_SIZE], char uuid[UUID_SIZE]){
  strncpy(email->from, from, EMAIL_SIZE);
  char unregister_text[KB] = "";
  strncpy(email->to, to, EMAIL_SIZE);
  strncpy(email->subject, title, KB);
  snprintf(unregister_text, KB, UNREGISTER_TEMPLATE, SERVER_URL, uuid);
  char date_str[50];
  get_current_time(date_str);

  snprintf(
    email->body, EMAIL_BODY_SIZE,
    "Date: %s\r\n" \
    "To: %s\r\n" \
    "From: %s\r\n" \
    "Subject: %s\r\n" \
    "Content-Type: text/html; charset=\"latin-1\"\n" \
    "Mime-version: 1.0\n" \
    "\r\n" /* empty line to divide headers from body, see RFC5322 */ \
    "%s\r\n"\
    "%s\r\n",
    date_str, to, from, title, content, unregister_text
  );
  return 0;
}

static int send_email(EmailConfig * config, Email * email){
  #ifdef NO_EMAIL
  printf("%s", email->body);
  return 0;
  #endif
  CURL *curl;
  curl = curl_easy_init();
  if(!curl) {
    printf("Failed to initialize curl for sending email");
    return -1;
  }

  curl_easy_setopt(curl, CURLOPT_USERNAME, config->username);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, config->password);
  curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

  #ifdef RELEASE
  curl_easy_setopt(curl, CURLOPT_CAINFO, EMAIL_SSL_CERT_PATH);
  #endif

  curl_easy_setopt(curl, CURLOPT_URL, EMAIL_SERVER);
  curl_easy_setopt(curl, CURLOPT_MAIL_FROM, email->from);

  struct curl_slist *recipients = curl_slist_append(NULL, email->to);
  curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA, email);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

  #if DEBUG
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  #endif

  /* Send the message */
  email->progress = 0;
  CURLcode res = curl_easy_perform(curl);
  if(res != CURLE_OK){
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
  }

  curl_slist_free_all(recipients);
  curl_easy_cleanup(curl);
  return (int)res;
}

typedef enum EmailType {SINGLE, GAME} EmailType;
typedef struct EmailNode EmailNode;
struct EmailNode {
  EmailType type;
  int game_id;
  Email email;
  EmailNode * next;
  EmailNode * prev;
  int retries;
};

typedef struct EmailDaemon {
  Pool pool;
  Arena arena;
  DB * db;
  EmailNode * head;
  EmailNode * tail;
  EmailConfig * config;
  bool is_running;
  pthread_t id;
  size_t time_between_emails;
  size_t sent_emails;
  pthread_mutex_t nodes_lock;
  pthread_mutex_t pool_lock;
} EmailDaemon;

#define MAX_RETRIES 3

void daemon_enqueue(EmailDaemon * daemon, EmailNode * node){
  if(!daemon->is_running)  {
    printf("Not adding more emails to queue, server is stopping.\n");
    return;
  }

  pthread_mutex_lock(&daemon->nodes_lock);
  if(daemon->tail == NULL){
    daemon->tail = node;
  }

  node->next = daemon->head;
  if(daemon->head != NULL){
    daemon->head->prev = node;
  }
  daemon->head = node;
  pthread_mutex_unlock(&daemon->nodes_lock);
}

EmailNode * daemon_dequeue(EmailDaemon * daemon){
  pthread_mutex_lock(&daemon->nodes_lock);
  EmailNode * node = daemon->tail;
  daemon->tail = node->prev;
  if(daemon->tail == NULL){
    daemon->head = NULL;
  }
  pthread_mutex_unlock(&daemon->nodes_lock);
  return node;
}

static void * daemon_run(void * arg){
  EmailDaemon * daemon = (EmailDaemon *) arg;
  while(daemon->is_running || daemon->tail != NULL){
    sleep(daemon->time_between_emails);
    if(daemon->tail == NULL){
      continue;
    }

    EmailNode * node = daemon_dequeue(daemon);

    if(node->type == SINGLE){
      // TODO: Mark registration as invalid if the email doesn't exist.
      // Some interaction with the email server may be possible?

      int result = send_email(daemon->config, &node->email);
      if(result != 0){
        node->retries++;
        printf("Failed to send the wanted email code %d\n", result);
        if(node->retries < MAX_RETRIES){
          daemon_enqueue(daemon, node);
          sleep(10);
          // TODO: Failed probably on our side but still better to delete registration
          continue;
        }
        printf("Max retries tried, discarding email to %s\n", node->email.to);
      } else {
        daemon->sent_emails++;
      }
      node->retries = 0;
    }
    else {
      reset_arena(&daemon->arena);
      // TODO: We should take advantage of the connection to server being open
      // and send multiple emails at once, instead of closing and opening it
      // every time.

      RegistrationsList list;
      int result = read_confirmed_game_registrations(daemon->db, &daemon->arena, node->game_id, &list);
      if(result == ERR_DB_DISCONNECTED){
        daemon_enqueue(daemon, node);
        printf("Database is down, waiting to see if it recovers\n");
        sleep(10);
        continue;
      }

      Email * template = &node->email;
      Email * email = alloc_in_arena(&daemon->arena, sizeof(Email));
      for(int i=0; i < list.total; i++){
        char * to = list.registrations[i].email;
        memset(email, 0, sizeof(Email));
        build_email(email, template->from, to, template->subject, template->body, list.registrations[i].uuid);
        int result = send_email(daemon->config, email);
        if(result != 0){
          printf("Failed to send email to %s with the code %d\n", to, result);
          continue;
        }
        daemon->sent_emails++;
      }
    }

    pthread_mutex_lock(&daemon->pool_lock);
    free_in_pool(&daemon->pool, node);
    pthread_mutex_unlock(&daemon->pool_lock);
  }

  // TODO: Store pending commands in a file so we can do them when the server starts
  pthread_exit(0);
}

// TODO: Unit test email daemon
int queue_group_email(EmailDaemon * daemon, char * subject, char * body, int game_id){
  pthread_mutex_lock(&daemon->pool_lock);
  EmailNode * node = alloc_in_pool(&daemon->pool);
  pthread_mutex_unlock(&daemon->pool_lock);
  if(node == NULL){
    printf("Queue email failed, maximum capacity reached!\n");
    return -1;
  }

  strncpy(node->email.from, daemon->config->username, EMAIL_SIZE);
  strncpy(node->email.subject, subject, KB);
  strncpy(node->email.body, body, EMAIL_BODY_SIZE);
  node->type = GAME;
  node->game_id = game_id;

  daemon_enqueue(daemon, node);
  return 0;
}

int queue_email(EmailDaemon * daemon, Email * email){
  pthread_mutex_lock(&daemon->pool_lock);
  EmailNode * node = alloc_in_pool(&daemon->pool);
  pthread_mutex_unlock(&daemon->pool_lock);
  if(node == NULL){
    printf("Queue email failed, maximum capacity reached!\n");
    return -1;
  }

  node->type = SINGLE;
  memcpy(&node->email, email, sizeof(Email));

  daemon_enqueue(daemon, node);
  return 0;
}

int init_email_daemon(EmailDaemon * daemon, EmailConfig * config, int capacity, size_t time_between_emails, DB * db){
  daemon->head = NULL;
  daemon->tail = NULL;
  daemon->sent_emails = 0;
  daemon->time_between_emails = time_between_emails;
  daemon->is_running = true;
  daemon->config = config;
  daemon->db = db;
  int result = init_pool(&daemon->pool, sizeof(EmailNode), capacity);
  if(result != 0){
    printf("Memory Pool error, failed to initialize the email daemon\n");
    return -1;
  }

  result = init_arena(&daemon->arena, 50 * MB);
  if(result != 0){
    printf("Memory arena error, failed to initialize the email daemon\n");
    return -2;
  }

  if(pthread_mutex_init(&daemon->nodes_lock, NULL) != 0){
    printf("Failed to initialize the daemon nodes mutex\n");
    return -3;
  }

  if(pthread_mutex_init(&daemon->pool_lock, NULL) != 0){
    printf("Failed to initialize the daemon pool mutex\n");
    return -4;
  }

  if(pthread_create(&daemon->id, NULL, daemon_run, daemon) != 0){
    printf("Failed to initialize the email daemon thread\n");
    return -5;
  }

  return 0;
}

int shutdown_email_daemon(EmailDaemon * daemon){
  daemon->is_running = false;
  pthread_mutex_destroy(&daemon->nodes_lock);
  pthread_mutex_destroy(&daemon->pool_lock);
  pthread_join(daemon->id, NULL);
  free_pool(&daemon->pool);
  free_arena(&daemon->arena);
}

#endif
