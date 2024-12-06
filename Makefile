# SPDX-License-Identifier: CC0-1.0
# Copyright (C) 2024 Tianhao Wang <i@shrik3.com>
# I don't really care about my part of "copyright" but I'm adding this
# to make it compliant with REUSE Specification
#
# I'm here to do evil things: instead of letting automake generate my
# makefile, I let my makefile call autoconf. I seriously don't want all
# that stuffs in this simple library (I don't need that much portability
# for my purposes)
#
# I only need autoconf to 1. do some sanity check and 2. produce a
# config.h for me.

SRC = pcl.c private/pcl_private.c private/pcl_version.c
OBJS = $(patsubst %.c, %.o,  $(SRC))
LDFLAGS = -lpthread
HACKS = -MD -MP -
CFLAGS = -O2
CC = gcc


ifeq ($(CO_MODE), UCONTEXT)
	_CO_MODE := -DCO_USE_UCONTEXT
else
	_CO_MODE := -DCO_USE_LONGJMP
endif

ifeq ($(USE_MT), Y)
_CO_MT ?= -DCO_MULTI_THREAD
endif

_CO_MODE ?= -DCO_USE_LONGJMP
DEFINES = $(_CO_MODE) $(_CO_MT)

.PHONY: all
all: libpcl.a

libpcl.a: $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $^ -c -o $@

.PHONY: test
test:
	make -C test

.PHONY: clean
clean:
	rm -f private/*.o
	rm -f *.o libpcl.a
	make -C test clean


# this is ragher implicit: if `configure` exists it means you are looking at a
# tarball release and the configure script is already shipped. Otherwise from
# the maintainer side, run autoreconf to generate all the duct tapes
config: configure
	./configure

configure:
	autoreconf

GNU_LITTER = config.* configure configure~
dist-clean:
	rm -f $(GNU_LITTER)
	rm -rf autom4te.cache
	rm -rf autosomething
