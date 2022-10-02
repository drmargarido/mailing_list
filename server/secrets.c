#ifndef SECRETS_INCLUDED
#define SECRETS_INCLUDED

#define EMAIL_SERVER "smtp://mail.example.com:587"
#define EMAIL_USERNAME "user@example.com"
#define EMAIL_PASSWORD ""
#define EMAIL_SSL_CERT_PATH ""

#define API_TOKEN ""
#define DATABASE_CONNECTION_STRING ""
#define SERVER_URL "http://127.0.0.1:8080"

#ifdef RELEASE
  #include "environments/prod.c"
#elif TESTING
  #include "environments/test.c"
#else
  #include "environments/dev.c"
#endif


#endif
