CC = cc
STD = -std=c99 -pedantic
W = -W -Wall
OPT ?= -O2
DEBUG = -g -ggdb
CFLAGS = $(STD) $(OPT) $(W) $(DEBUG)
UNAME_S := $(shell sh -c 'uname -s 2>/dev/null || echo nope')

# jemalloc
JEMALLOC_DIR = ../deps/jemalloc
JEMALLOC_LIB = $(JEMALLOC_DIR)/lib/libjemalloc.a
JEMALLOC_INC = $(JEMALLOC_DIR)/include/jemalloc

CFLAGS += -I$(JEMALLOC_INC)
DEPS_LIBS = $(JEMALLOC_LIB) -ldl
MINUNIT_LIBS = -lm
ifeq ($(UNAME_S),Linux)
	DEPS_LIBS+= -pthread
	MINUNIT_LIBS += -lrt
endif

TESTS = test_dict

all: test

.PHONY: test clean

clean:
	rm -rf $(TESTS) *.o

test: $(TESTS)
	@$(foreach test,$(TESTS), ./$(test);)

test_dict: test_dict.c ../src/rr_dict.o ../src/adlist.o ../src/rr_malloc.o
	$(CC) $(CFLAGS) -o $@ $^ $(DEPS_LIBS) $(MINUNIT_LIBS)
