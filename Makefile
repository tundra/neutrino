# Build everything.
main:	ctrino


# Run all tests.
tests:	tests-python


PYTHON_TEST_FILES=$(shell find tests/python -name "[^_]*.py")
PYTHON_TESTS=$(patsubst %, test-%, $(PYTHON_TEST_FILES))


# Runs the python tests.
tests-python:	$(PYTHON_TESTS)


# Individual python tests.
$(PYTHON_TESTS): test-%: %
	@PYTHONPATH=$(PYTHONPATH):src/python \
		python $<


C_SRCS=$(shell find src/c -name "*.c")
C_HDRS=$(shell find src/c -name "*.h")
C_OBJS=$(patsubst %.c, %.o, $(C_SRCS))
CFLAGS=-Wall -ansi -pedantic -O0 -g


# Build all things C.
ctrino:	$(C_OBJS)
	gcc $(C_OBJS) -o $@


# Individual object files.
$(C_OBJS): %.o: %.c $(C_HDRS)
	gcc $(CFLAGS) -c $< -o $@


clean:
	rm -f $(C_OBJS)


.PHONY:	clean
