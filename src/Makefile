CC	= cc
W	= -W -Wall
OPT = -std=c99 -pedantic -O2 -g -ggdb
SDSHEADER = -I../deps/sds
CFLAGS	= $(OPT) $(W) $(XCFLAGS) $(SDSHEADER)
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

rhino-rox: rr_main.o rr_logging.o sds.o
	$(CC) $(CFLAGS) -o $@ $^

rr_main.o:
	$(CC) $(CFLAGS) -c rr_main.c

sds.o: ../deps/sds/sds.c
	$(CC) $(CFLAGS) -c $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<
