CC=gcc
CFLAGS=-c  
LDFLAGS=-g -Wall -lpthread -lm -lasound -lmpg123
SOURCES = deamon.c arg.c start.c udpsocket.c  funcs.c thrd.c audio.c inotify.c
OBJECTS = $(SOURCES:.c=.o)
BINARY = binary




all:$(SOURCES) $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	rm -f ./*.o
	rm -f /run/tese.sock
	./binary


.c.o:
	$(CC) $(CFLAGS)   $< -o $@ 

clean:
	rm -f ./*.o
	rm -f /run/tese.sock
