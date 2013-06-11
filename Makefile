BIN=bin
MAIN=$(BIN)/ctrino


# Build everything.
main:	$(MAIN)


# Run all tests.
tests:	tests-python tests-c


PYTHON_TEST_FILES=$(shell find tests/python -name "[^_]*.py")
PYTHON_TESTS=$(patsubst %, test-%, $(PYTHON_TEST_FILES))


# Runs the python tests.
tests-python:	$(PYTHON_TESTS)


# Individual python tests.
$(PYTHON_TESTS): test-%: %
	@echo Running $<
	@PYTHONPATH=$(PYTHONPATH):src/python \
		python $<


C_SRCS=$(shell find src/c -name "*.c")
C_HDRS=$(shell find src/c -name "*.h")
C_OBJS=$(patsubst %.c, bin/%.o, $(C_SRCS))

C_TEST_LIB_SRCS=tests/c/test.c
C_TEST_LIB_OBJS=$(BIN)/tests/c/test.o
C_TEST_SRCS=$(shell find tests/c -name "test_*.c")
C_TEST_HDRS=$(shell find tests/c -name "*.h")
C_TEST_OBJS=$(patsubst %.c, $(BIN)/%.o, $(C_TEST_SRCS))
C_TEST_EXES=$(patsubst %.c, $(BIN)/%, $(C_TEST_SRCS))
C_TEST_GEN_SRC=$(BIN)/tests/c/toc.c
C_TESTS=$(patsubst %, test-%, $(C_TEST_EXES))

ALL_C_OBJS=$(C_OBJS) $(C_TEST_OBJS) $(C_TEST_LIB_OBJS)

C_DIALECT_FLAGS=-Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function
C_ENV_FLAGS=-Isrc/c -I$(BIN)/tests/c
CFLAGS=$(C_DIALECT_FLAGS) $(C_ENV_FLAGS) -O0 -g


# Build all things C.
$(MAIN):$(C_OBJS)
	@mkdir -p $(shell dirname $@)
	@echo Building $(MAIN)
	@gcc $(C_OBJS) -o $@


# Individual object files.
$(ALL_C_OBJS): $(BIN)/%.o: %.c $(C_HDRS) $(C_TEST_HDRS) $(C_TEST_GEN_SRC)
	@mkdir -p $(shell dirname $@)
	@echo Compiling $<
	@gcc $(CFLAGS) -c $< -o $@


$(C_TEST_GEN_SRC): $(C_TEST_SRCS)
	@mkdir -p $(shell dirname $@)
	@echo Generating test table of contents
	@cat $(C_TEST_SRCS) \
	  | grep "TEST\(.*\)" \
	  | sed "s/\(TEST(.*)\).*/DECLARE_\1;/g" \
	  > $@
	@echo "ENUMERATE_TESTS_HEADER {" \
	  >> $@
	@cat $(C_TEST_SRCS) \
	  | grep "TEST\(.*\)" \
	  | sed "s/\(TEST(.*)\).*/  ENUMERATE_\1;/g" \
	  >> $@
	@echo "}" \
	  >> $@


$(C_TEST_EXES): %: %.o $(C_TEST_LIB_OBJS)
	@mkdir -p $(shell dirname $@)
	@echo Building $@
	@gcc $(C_TEST_LIB_OBJS) $< -o $@


tests-c:$(C_TESTS)


# Individual C tests.
$(C_TESTS):test-%: %
	@echo Running $<
	@./$<


clean:
	@echo Cleaning $(BIN)
	@rm -rf $(BIN)


.PHONY:	clean tests-c tests-python
