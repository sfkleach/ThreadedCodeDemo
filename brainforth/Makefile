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
CCFLAGS=-Wall -Werror -g -Og -std=c++17

json.hpp:
	curl --silent --show-error https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp > $@

brainforth_runner: brainforth_runner.cpp json.hpp
	$(CC) $(CCFLAGS) -o $@ $<

brainforth_compiler: brainforth_compiler.cpp json.hpp
	$(CC) $(CCFLAGS) -o $@ $<

brainforth_tokeniser: brainforth_tokeniser.cpp json.hpp
	$(CC) $(CCFLAGS) -o $@ $<

.PHONY: all
all: brainforth_runner brainforth_compiler brainforth_tokeniser

.PHONY: release
release: CCFLAGS=-Wall -O3 -std=c++17
release: brainforth_runner brainforth_compiler brainforth_tokeniser

.PHONY: clean
clean:
	rm -f brainforth_runner
	rm -f brainforth_compiler
	rm -f brainforth_tokeniser
	rm -f json.hpp
