CC = gcc
CFLAGS = -I.
DEPS = rdt.h

all: receiver sender

receiver: Receiver.o rdt.o
	$(CC) -o $@ $^ $(CFLAGS)

sender: Sender.o rdt.o
	$(CC) -o $@ $^ $(CFLAGS)
	cp -f $@ ./test/$@

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean
clean:
	rm -f *.o
	rm -f receiver
	rm -f sender
	rm -f ./test/sender
