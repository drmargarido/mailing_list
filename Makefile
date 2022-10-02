all: build-dependencies
	mkdir -p bin
	gcc -o bin/list-server -g server/main.c vendor/libhttp/lib/libhttp.a  -lpq `pkg-config --cflags libpq` -lcurl -lpthread -ldl -Ivendor/libhttp/include/
	gcc -o bin/list-client -g -O2 client/main.c -lcurl `pkg-config --cflags gtk+-3.0` `pkg-config --libs gtk+-3.0`
	gcc -o bin/list-cron -g -O2 cron/main.c -lpq `pkg-config --cflags libpq`

debug: build-dependencies
	mkdir -p bin
	gcc -o bin/list-server -g -DDEBUG server/main.c vendor/libhttp/lib/libhttp.a -lpq `pkg-config --cflags libpq` -lcurl -lpthread -ldl -Ivendor/libhttp/include/
	gcc -o bin/list-client -g -O2 client/main.c -lcurl `pkg-config --cflags gtk+-3.0` `pkg-config --libs gtk+-3.0`
	gcc -o bin/list-cron -g -O2 cron/main.c -lpq `pkg-config --cflags libpq`

release: build-dependencies
	mkdir -p bin
	gcc -o bin/list-server -DRELEASE -O2 server/main.c vendor/libhttp/lib/libhttp.a -lpq `pkg-config --cflags libpq` -lcurl -lpthread -ldl -Ivendor/libhttp/include/
	gcc -o bin/list-client -DRELEASE -O2 client/main.c -lcurl `pkg-config --cflags gtk+-3.0` `pkg-config --libs gtk+-3.0`
	gcc -o bin/list-cron -DRELEASE -O2 cron/main.c -lpq `pkg-config --cflags libpq`

build-dependencies:
	cd vendor/libhttp/ && make

test:
	bash run_tests.sh

install-client:
	mkdir -p /usr/share/mailing-list/images
	cp -R client/setup/mailing_list.png /usr/share/pixmaps/mailing-list.png
	cp -R client/setup/mailing_list.png /usr/share/mailing-list/images/mailing-list_512x512.png

	cp -R bin/list-client /usr/bin/mailing-list
	cp -R client/setup/mailing_list.desktop /usr/share/applications/mailing-list.desktop
	chmod +x /usr/bin/mailing-list
	@echo "Client Installed!"

uninstall-client:
	rm -R /usr/bin/mailing-list
	rm -R /usr/share/pixmaps/mailing-list.png
	rm -R /usr/share/mailing-list
	rm -R /usr/share/applications/mailing-list.desktop

clean:
	rm -rf tmp
	rm -f bin/*
