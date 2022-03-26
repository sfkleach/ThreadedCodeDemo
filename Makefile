# Essential warning.
MAKEFLAGS+=--warn-undefined-variables

# Causes the commands in a recipe to be issued in the same shell (beware cd commands not executed in a subshell!)
.ONESHELL:

SHELL:=/bin/bash

# When using ONESHELL, we want to exit on error (-e) and error if a command fails in a pipe (-o pipefail)
# When overriding .SHELLFLAGS one must always add a tailing `-c` as this is the default setting of Make.
.SHELLFLAGS:=-e -o pipefail -c

# Invoke the all target when no target is explicitly specified.
.DEFAULT_GOAL:=all

# Delete targets if their recipe exits with a non-zero exit code.
.DELETE_ON_ERROR:

CC=g++
CCFLAGS=-Wall -g -Og -std=c++17

.PHONY: all
all: direct_threading_demo subroutine_threading_demo

.PHONY: release
release: CCFLAGS=-Wall -O3 -std=c++17
release: direct_threading_demo subroutine_threading_demo

.PHONY: clean
clean:
	rm -f direct_threading_demo subroutine_threading_demo

direct_threading_demo: direct_threading_demo.cpp
	$(CC) $(CCFLAGS) -o $@ $^

subroutine_threading_demo: subroutine_threading_demo.cpp
	$(CC) $(CCFLAGS) -o $@ $^

