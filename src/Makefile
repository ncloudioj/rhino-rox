CC	= gcc
W	= -W -Wall -W -Wbad-function-cast
OPT = -std=c99 -O2 -g
CFLAGS	= $(OPT) $(W) $(XCFLAGS)
prefix	= /usr/local
IPROGS	= rhino-rox
PROGS   = $(IPROGS)


all: $(PROGS)

install: $(PROGS)
	for f in $(IPROGS); do cp $$f $(DESTDIR)$(prefix)/bin; done

clean:
	rm -rf $(PROGS) *.[ao] *.so

test: all
	cd tests && ./run-test.sh

rhino-rox: rr_main.o rr_logging.o
	$(CC) $(CFLAGS) -o $@ $^

rr_main.o:
	$(CC) $(CFLAGS) -c rr_main.c

%.o: %.c
	$(CC) $(CFLAGS) -c $<
