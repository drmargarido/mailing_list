#ifndef SECRETS_INCLUDED
#define SECRETS_INCLUDED

#define API_TOKEN ""
#define SERVER_URL "http://127.0.0.1:8080"

#ifdef RELEASE
  #include "environments/prod.c"
#else
  #include "environments/dev.c"
#endif


#endif
