# Mailing Lists

## Download Repository

Clone with submodules for the dependencies of each part
```bash
git clone --recurse-submodules git@bitbucket.org:Alface0/mailinglists.git
```

## Server

### Functionality

1. Allow register in a game mailing list.
2. Send email to users to confirm that they joined the mailling list.
3. Allow registered users to unsubscribe if they want.

NOTE: There is no endpoint to add new games to the database, it needs to be done
with the `create_game` function that exists in the `server/database.c` file or
with a query directly in the database.


### Setup Development Process

1. Create postgres database with name *maling_list*.
2. Edit the file `server/environments/dev.c` with the postgres database user password.
2. Run migrations.
```bash
psql -U postgres -d mailing_list -h 127.0.0.1 -f server/migrations/001_init.sql
psql -U postgres -d mailing_list -h 127.0.0.1 -f server/migrations/002_add_confirm.sql
```
3. Install libpq.
4. Install libcurl.
5. Compile with `make`.
5. Run the server `bin/list-server`.


#### Production Deploy

1. Fill the files `server/environments/prod.c` and `client/environments/prod.c` with your wanted configurations.
2. Compile the wanted binaries with the `make release` command.
3. Create the user `lists`.
4. Copy the server binary to the `/opt/mailing_list` folder.
5. Make sure the user can execute the binaries on the `/opt/mailing_list` folder.
6. Create the database `mailing_list` and run the following migrations.
```bash
psql -U mailing_list -d mailing_list -h 127.0.0.1 -f server/migrations/001_init.sql
psql -U mailing_list -d mailing_list -h 127.0.0.1 -f server/migrations/002_add_confirm.sql
```
7. Create the user `mailing_list` in the database with access to the `mailing_list` database.
8. Copy the systemd service to the `/etc/systemd/system` folder.
9. Enable and activate the service.
10. Edit the nginx configuration with the server domain and SSL certs and add it to the `/etc/nginx/sites-available` folder.
11. Link the file to the `/etc/nginx/sites-enabled/` folder and restart nginx.
12. If we don't have the ssl certificates yet let only the port 80 part in the configuration and run the certbot.
13. Block the 8080 port in the firewall for external requests so the internal server is not exposed.
```bash
/sbin/iptables -A INPUT -s 127.0.0.1 -p tcp --destination-port 8080 -j ACCEPT
/sbin/iptables -A INPUT -p tcp --destination-port 8080 -j DROP
```
14. Copy the cron binary to the `/opt/mailing_list` folder.
15. Make sure the user `lists` is the owner of the cron binary.
16. Copy the `cron/mailing_list` file to `/etc/cron.d/mailing_list`.

### Dependencies

+ libpq - [link](https://www.postgresql.org/docs/9.5/libpq.html)
+ libhttp - [link](https://github.com/lammertb/libhttp)
+ libcurl - [link](https://curl.se/libcurl/)

### Testing

#### Run

1. For now the user for testing in the database is `postgres` with the password
`123qweasd.`.
2. Run the following command:
```bash
make test
```


## Client

### functionality

1. Consult mailing list by game.
2. Send email to all users of a mailing list.

### Dependencies

+ gtk - [link](https://www.gtk.org/)
+ jsmn - [link](https://github.com/zserge/jsmn)
