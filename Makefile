# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = gcc
CFLAGS = -g -Wall -O0
LDFLAGS = -lpthread

all: proxy echoclient echoserveri dd2hex hex2dd

test: test.o csapp.o proxylib.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo run test code automatically
	./$@

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o
	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

proxylib.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -DLIB -c proxy.c -o $@

echoclient.o: echoclient.c csapp.h
echoserveri.o: echoserveri.c csapp.h
dd2hex.o: dd2hex.c csapp.h
hex2dd.o: hex2dd.c csapp.h
test.o: test.c csapp.h
	$(CC) $(CFLAGS) -DTEST $^ -c


echoclient: echoclient.o csapp.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

echoserveri: echoserveri.o csapp.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

dd2hex: dd2hex.o csapp.o
hex2dd: hex2dd.o csapp.o

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o echoclient echoserveri proxy core *.tar *.zip *.gzip *.bzip *.gz

	