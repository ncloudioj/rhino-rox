CC = cc
W = -W -Wall
OPT = -std=gnu99 -pedantic -O2 -g -ggdb
CFLAGS = $(OPT) $(W) $(XCFLAGS)
PREFIX ?= /usr/local
PROGS = rhino-rox

# jemalloc 
JEMALLOC_DIR = ../deps/jemalloc
JEMALLOC_LIB = $(JEMALLOC_DIR)/lib/libjemalloc.a
JEMALLOC_INC = $(JEMALLOC_DIR)/include/jemalloc

CFLAGS+= -I$(JEMALLOC_INC)
DEPS_LIBS= $(JEMALLOC_LIB) -ldl -pthread

all: jemalloc
	
.PHONY: jemalloc update_deps

$(JEMALLOC_LIB): $(JEMALLOC_DIR)/Makefile
	cd $(JEMALLOC_DIR) && $(MAKE) CC=$(CC) 

$(JEMALLOC_DIR)/autogen.sh:
	git submodule update --init

$(JEMALLOC_DIR)/Makefile: | $(JEMALLOC_DIR)/autogen.sh
	cd $(JEMALLOC_DIR) && ./autogen.sh --with-jemalloc-prefix=je_ --disable-valgrind

jemalloc: $(JEMALLOC_LIB)

update_deps:
	rm -rf $(JEMALLOC_DIR) && git submodule update --init

all: $(PROGS)

install: $(PROGS)
	for f in $(PROGS); do cp $$f $(PREFIX)/bin; done

.PHONY: clean

clean:
	rm -rf $(PROGS) *.[ao] *.so

test: all
	cd tests && ./run-test.sh

rhino-rox: rr_main.o rr_logging.o sds.o rr_malloc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(DEPS_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<
