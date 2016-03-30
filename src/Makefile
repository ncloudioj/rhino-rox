CC = cc
W = -W -Wall
OPT = -std=c99 -pedantic -O2 -g -ggdb
CFLAGS = $(OPT) $(W) $(XCFLAGS)
UNAME_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
PREFIX ?= /usr/local
PROGS = rhino-rox t

# jemalloc 
JEMALLOC_DIR = ../deps/jemalloc
JEMALLOC_LIB = $(JEMALLOC_DIR)/lib/libjemalloc.a
JEMALLOC_INC = $(JEMALLOC_DIR)/include/jemalloc

CFLAGS += -I$(JEMALLOC_INC)
DEPS_LIBS = $(JEMALLOC_LIB) -ldl
ifeq ($(UNAME_S),linux)
	DEPS_LIBS+= -pthread
endif

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

rhino-rox: rr_server.o rr_logging.o sds.o adlist.o rr_malloc.o rr_event.o rr_array.o \
	rr_minheap.o rr_datetime.o rr_network.o rr_replying.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(DEPS_LIBS)

t: rr_main.o rr_logging.o sds.o rr_malloc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(DEPS_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<
