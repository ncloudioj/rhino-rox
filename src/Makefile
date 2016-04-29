CC = cc
STD = -std=c99 -pedantic
W = -W -Wall
# OPT is Overwritable to disable the optimizations
OPT ?= -O2
DEBUG = -g -ggdb
CFLAGS = $(STD) $(OPT) $(W) $(DEBUG)
UNAME_S := $(shell sh -c 'uname -s 2>/dev/null || echo nope')
PREFIX ?= /usr/local
PROGS = rhino-rox

# jemalloc
JEMALLOC_DIR = ../deps/jemalloc
JEMALLOC_LIB = $(JEMALLOC_DIR)/lib/libjemalloc.a
JEMALLOC_INC = $(JEMALLOC_DIR)/include/jemalloc

CFLAGS += -I$(JEMALLOC_INC)
DEPS_LIBS = $(JEMALLOC_LIB) -ldl
ifeq ($(UNAME_S),Linux)
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

rhino-rox: rr_server.o rr_logging.o sds.o adlist.o rr_malloc.o rr_event.o rr_array.o \
	rr_minheap.o rr_datetime.o rr_network.o rr_replying.o rr_config.o rr_bgtask.o ini.o \
	rr_dict.o robj.o sha1.o util.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(DEPS_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

.PHONY: clean cleanall valgrind

clean:
	rm -rf $(PROGS) *.o

cleanall: clean
ifneq (,$(wildcard $(JEMALLOC_DIR)/Makefile))
	-(cd $(JEMALLOC_DIR) && $(MAKE) distclean)
endif

valgrind:
	$(MAKE) OPT="-O0"
	valgrind --leak-check=yes --log-file=valgrind.log ./rhino-rox
