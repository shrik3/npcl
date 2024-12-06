# SPDX-License-Identifier: CC0-1.0
# Copyright (C) 2024 Tianhao Wang <i@shrik3.com>
# I don't really care about my part of "copyright" but I'm adding this
# to make it compliant with REUSE Specification

tests = cobench cothread

PCL_INCLUDE = ../pcl.h
LIBPCL = ../libpcl.a

.PHONY: all clean
all: $(tests)

test_npcl: test_npcl.c
	gcc -o $@ $^

clean:
	rm -f *.o $(tests) test_npcl

%: %.c
	gcc -o $@ $^ $(LIBPCL)  -lpthread -include $(PCL_INCLUDE)

