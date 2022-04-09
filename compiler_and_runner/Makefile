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

json.hpp:
	curl --silent --show-error https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp > $@

cisc_runner_demo: cisc_runner_demo.cpp json.hpp
	$(CC) $(CCFLAGS) -o $@ $<

cisc_compiler_demo: cisc_compiler_demo.cpp json.hpp
	$(CC) $(CCFLAGS) -o $@ $<

.PHONY: all
all: cisc_runner_demo cisc_compiler_demo

.PHONY: clean
clean:
	rm -f cisc_runner
	rm -f cisc_compiler_demo
	rm -f demo json.hpp
