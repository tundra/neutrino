# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

MACHINE=64

# Toggle the valgrind command depending on the VALGRIND variable.
VALGRIND=off

# Configurable emulator command that can be used to run tests on an emulator.
EMULATOR_CMD=

# Whether to compile in optimized or debug mode.
MODE=dbg

# Whether to treat warnings as errors or just warnings.
STRICT_ERRORS=on

# Whether to execute the CHECK macros or disable them.
CHECKS=on

# Which flavor of OS we're on.
OS=posix

-include config/$(CONFIG).cfg

ifeq ($(VALGRIND),on)
  VALGRIND_CMD=valgrind -q --leak-check=full --error-exitcode=1
else
  VALGRIND_CMD=
endif

# If there's a config use that as part of the bindir name, otherwise build one
# based on the settings.
ifneq ($(CONFIG),)
  BIN=bin-$(CONFIG)
else
  BIN=bin-$(MACHINE)-$(MODE)
endif

OUT=$(BIN)/out

# Default target.
main:	all-c


# Convenience alias for building everything.
all:	main


# Run all tests.
test:	lint test-python test-c test-golden


# Dependencies for all targets.
GLOBAL_DEPS=Makefile


PYTHON_TEST_FILES=$(shell find tests/python -name "[^_]*.py")
PYTHON_TEST_RUNS=$(patsubst tests/python/%, test-python-%, $(PYTHON_TEST_FILES))


# Runs the python tests.
test-python:	$(PYTHON_TEST_RUNS)


# Individual python tests.
$(PYTHON_TEST_RUNS): test-python-%: tests/python/% $(GLOBAL_DEPS)
	@echo Running $<
	@PYTHONPATH=$(PYTHONPATH):src/python \
		python $<


# Decide the optimization mode based on the MODE parameter.
ifeq ($(MODE),dbg)
  OPT_FLAGS=-O0 -g
else
  OPT_FLAGS=-O3
endif


DEFINES=-DM$(MACHINE)=1

# Decide whether to define ENABLE_CHECKS based on the CHECKS variable.
ifeq ($(CHECKS),on)
  DEFINES := $(DEFINES) -DENABLE_CHECKS=1
endif

# Set a define based on os.
OS_UPPER_CASE=$(shell echo $(OS) | tr a-z A-Z)
DEFINES := $(DEFINES) -DOS_$(OS_UPPER_CASE)=1


# Configuration of the C language dialect to use.
C_DIALECT_FLAGS=-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -std=c99
ifeq ($(STRICT_ERRORS),on)
  C_DIALECT_FLAGS := $(C_DIALECT_FLAGS) -Werror
endif


C_ENV_FLAGS=-Isrc/c -I$(BIN)/tests/c
CFLAGS=$(C_DIALECT_FLAGS) $(C_ENV_FLAGS) $(OPT_FLAGS) -m$(MACHINE) $(DEFINES)
LINKFLAGS=-m$(MACHINE) -rdynamic -lrt


# The library part of ctrino, that is, everything but main.
C_MAIN_NAME=main.c
C_LIB_SRCS=$(shell find src/c -name "*.c" -and -not -name $(C_MAIN_NAME) -and -not -name "*-opt.c" | sort)
C_LIB_HDRS=$(shell find src/c -name "*.h" | sort)
C_LIB_OBJS=$(patsubst %.c, $(BIN)/%.o, $(C_LIB_SRCS))
C_LIB_DEPS=$(C_LIB_HDRS) $(GLOBAL_DEPS)


# The C library object files.
$(C_LIB_OBJS): $(BIN)/%.o: %.c $(C_LIB_DEPS)
	@mkdir -p $(shell dirname $@)
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $< -o $@


# The main part of ctrino.
C_MAIN_SRCS=$(shell find src/c -name "*.c" -and -name $(C_MAIN_NAME) | sort)
C_MAIN_OBJS=$(patsubst %.c, $(BIN)/%.o, $(C_MAIN_SRCS))
C_MAIN_DEPS=$(C_LIB_DEPS)
C_MAIN_EXE=$(BIN)/ctrino


# The main object files.
$(C_MAIN_OBJS): $(BIN)/%.o: %.c $(C_MAIN_DEPS)
	@mkdir -p $(shell dirname $@)
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $< -o $@


# Build the ctrino executable.
$(C_MAIN_EXE): $(C_MAIN_OBJS) $(C_LIB_OBJS)
	@mkdir -p $(shell dirname $@)
	@echo Building $@
	@$(CC) $^ $(LINKFLAGS) -o $@


# The library parts of the tests, that it, everything but the test main.
C_TEST_MAIN_NAME=test.c
C_TEST_LIB_SRCS=$(shell find tests/c -name "*.c" -and -not -name $(C_TEST_MAIN_NAME) | sort)
C_TEST_LIB_HDRS=$(shell find tests/c -name "*.h" | sort)
C_TEST_LIB_OBJS=$(patsubst %.c, $(BIN)/%.o, $(C_TEST_LIB_SRCS))
C_TEST_LIB_RUNS=$(patsubst tests/c/test_%.c, test-c-%, $(C_TEST_LIB_SRCS))
C_TEST_LIB_OUTS=$(patsubst tests/c/test_%.c, $(OUT)/tests/c/test_%.out, $(C_TEST_LIB_SRCS))
C_TEST_LIB_DEPS=$(C_LIB_DEPS) $(C_TEST_LIB_HDRS)


# The test object files.
$(C_TEST_LIB_OBJS): $(BIN)/%.o: %.c $(C_TEST_LIB_DEPS)
	@mkdir -p $(shell dirname $@)
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $< -o $@


# The generated table of contents
C_TEST_TOC_SRCS=$(BIN)/tests/c/toc.c


# Build the table of contents. Yuck.
$(C_TEST_TOC_SRCS):	$(C_TEST_LIB_SRCS) $(GLOBAL_DEPS)
	@mkdir -p $(shell dirname $@)
	@echo Generating test table of contents
	@cat $(C_TEST_LIB_SRCS) \
	  | grep "TEST\(.*\)" \
	  | sed "s/\(TEST(.*)\).*/DECLARE_\1;/g" \
	  > $@
	@echo "ENUMERATE_TESTS_HEADER {" \
	  >> $@
	@cat $(C_TEST_LIB_SRCS) \
	  | grep "TEST\(.*\)" \
	  | sed "s/\(TEST(.*)\).*/  ENUMERATE_\1;/g" \
	  >> $@
	@echo "}" \
	  >> $@


# The main test runner.
C_TEST_MAIN_SRCS=$(shell find tests/c -name "*.c" -and -name $(C_TEST_MAIN_NAME) | sort)
C_TEST_MAIN_OBJS=$(patsubst %.c, $(BIN)/%.o, $(C_TEST_MAIN_SRCS))
C_TEST_MAIN_DEPS=$(C_TEST_LIB_DEPS) $(C_TEST_TOC_SRCS)
C_TEST_MAIN_EXE=$(BIN)/tests/c/main


# The main object files.
$(C_TEST_MAIN_OBJS): $(BIN)/%.o: %.c $(C_TEST_MAIN_DEPS)
	@mkdir -p $(shell dirname $@)
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $< -o $@


# Build all the tests into one executable. For obscure reasons the flags
# have to come after the objects.
$(C_TEST_MAIN_EXE): $(C_TEST_MAIN_OBJS) $(C_TEST_LIB_OBJS) $(C_LIB_OBJS)
	@mkdir -p $(shell dirname $@)
	@echo Building $@
	@$(CC) $^ $(LINKFLAGS) -o $@


EXEC_PREFIX=$(VALGRIND_CMD) $(EMULATOR_CMD)


# Run a C test and store the result in a file. This is kind of tricky because
# we want to both store the output and signal an error
$(C_TEST_LIB_OUTS):$(OUT)/tests/c/test_%.out:$(C_TEST_MAIN_EXE)
	@echo Running test_$*
	@mkdir -p $(shell dirname $@)
	@$(EXEC_PREFIX) ./$(C_TEST_MAIN_EXE) $* > $@ || touch $@.fail
	@cat $@ | ./src/sh/filter-backtrace.sh
	@if [ -f $@.fail ]; then rm $@ $@.fail; false; else true; fi


# Shorthand for running a C test.
$(C_TEST_LIB_RUNS):test-c-%:$(OUT)/tests/c/test_%.out


# Shorthand for running all the C tests.
test-c:$(C_TEST_LIB_RUNS)


# Build everything.
all-c:	$(C_MAIN_EXE) $(C_TEST_MAIN_EXE)


# The markdown files.
MD_SRCS=$(shell find -name "*.md" | sort)
MD_OBJS=$(patsubst %.md, $(BIN)/doc/%.html, $(MD_SRCS))


# The main object files.
$(MD_OBJS): $(BIN)/doc/%.html: %.md
	@mkdir -p $(shell dirname $@)
	@echo Generating $<
	@markdown $< > $@


docs:	$(MD_OBJS)


GOLDEN_SRCS=$(shell find tests/n/golden -name "*.gn" | sort)
GOLDEN_OUTS=$(patsubst tests/n/golden/%.gn, $(OUT)/tests/n/golden/%.out, $(GOLDEN_SRCS))
GOLDEN_RUNS=$(patsubst tests/n/golden/%.gn, test-golden-%, $(GOLDEN_SRCS))

# Shorthand for running individual golden tests.
$(GOLDEN_RUNS):test-golden-%:$(OUT)/tests/n/golden/%.out


# Run a golden test using the test runner script.
$(GOLDEN_OUTS):$(OUT)/tests/n/golden/%.out:tests/n/golden/%.gn $(C_MAIN_EXE)
	@echo -n Running golden test "$*: "
	@mkdir -p $(shell dirname $@)
	@./src/sh/run-golden-test.sh                                               \
	  -t "$<"                                                                  \
	  -o "$@"                                                                  \
	  -e "$(EXEC_PREFIX) $(C_MAIN_EXE)"


# Run all the golden tests.
test-golden:	$(GOLDEN_RUNS)


loc:
	@echo LOC \
	  C: `find . -name \*.[ch] | xargs cat | grep -v -e ^// -e ^$$ | wc -l` \
	  py: `find . -name \*.py | xargs cat | grep -v -e '^#' -e ^$$ | wc -l`


# Linting
lint-tabs:
	@echo Linting for tabs
	@! (find src/ tests/ -type f | grep -v ".*\.pyc" | xargs grep -P '\t')

lint:	lint-tabs

clean:
	@echo Cleaning
	@rm -rf bin-*


.PHONY:	clean test-c test-python test-golden $(C_TEST_LIB_RUNS) $(PYTHON_TEST_RUNS)
