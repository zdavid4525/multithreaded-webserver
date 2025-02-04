CC=clang
CFLAGS=-g
BINS=server
OBJS=server.o queue.o

all: $(BINS)

server: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -rf *.dSYM $(BINS)