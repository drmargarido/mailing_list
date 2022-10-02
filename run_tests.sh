#!/bin/bash
mkdir -p tmp

# Recreate database
export PGPASSWORD=123qweasd.
psql -h 127.0.0.1 -U postgres -c 'DROP DATABASE mailing_list_test;'
psql -h 127.0.0.1 -U postgres -c 'CREATE DATABASE mailing_list_test;'

psql -h 127.0.0.1 -U postgres -d mailing_list_test -f server/migrations/001_init.sql
psql -h 127.0.0.1 -U postgres -d mailing_list_test -f server/migrations/002_add_confirm.sql
psql -h 127.0.0.1 -U postgres -d mailing_list_test -c "INSERT INTO game(name) VALUES ('Shinobimatch');"

gcc -o tmp/test_allocator server/tests/test_allocator.c
gcc -g server/tests/test_database.c -lpq `pkg-config --cflags libpq` -o tmp/test_database
gcc -g -DTESTING server/tests/test_api.c vendor/libhttp/lib/libhttp.a -lcurl -lpq `pkg-config --cflags libpq` -o tmp/test_api -pthread -ldl -Ivendor/libhttp/include/
gcc -g -DTESTING server/tests/test_email.c -lcurl -lpq `pkg-config --cflags libpq` -lpthread -o tmp/test_email

for TEST_FILE in $(ls -1 tmp/)
do
  tmp/$TEST_FILE
  if [ $? -eq 0 ]
  then
    echo -e "\n$TEST_FILE - \e[32mPassed!\e[0m"
  else
    echo -e "\n$TEST_FILE - \e[31mFailed!\e[0m"
  fi
done

echo "All Tests Run"
