CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

all: server subscriber

server: server.c common.c -lm

subscriber: subscriber.c common.c

clean:
	rm -rf server subscriber *.o *.dSYM
