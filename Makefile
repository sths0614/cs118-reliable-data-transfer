CC = gcc
CFLAGS = -I.
DEPS = rdt.h

SRVPORT = 6812
CLIPORT = 6813
WINSIZE = 5
PL = 0.4
PC = 0.4
FILE1 = test1.bmp
FILE2 = test2.bmp
FILE3 = filler.txt

all: receiver sender

receiver: Receiver.o rdt.o
	$(CC) -o $@ $^ $(CFLAGS)

sender: Sender.o rdt.o
	$(CC) -o $@ $^ $(CFLAGS)
	cp -f $@ ./test/$@

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: test
test: all
	./test/sender $(SRVPORT) $(WINSIZE) $(PL) $(PC) &
	./receiver 127.0.0.1 $(SRVPORT) $(CLIPORT) $(WINSIZE) $(FILE1) $(PL) $(PC)
	./receiver 127.0.0.1 $(SRVPORT) $(CLIPORT) $(WINSIZE) $(FILE2) $(PL) $(PC)
	./receiver 127.0.0.1 $(SRVPORT) $(CLIPORT) $(WINSIZE) $(FILE3) $(PL) $(PC)

.PHONY: clean
clean:
	rm -f *.o
	rm -f receiver
	rm -f sender
	rm -f ./test/sender
