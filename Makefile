CC = gcc
CFLAGS = -I.
DEPS = rdt.h

all: client server

client: Receiver.o rdt.o
     $(CC) -o $@ $^ $(CFLAGS)

server: Sender.o rdt.o
     $(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
