PYTHON_TEST_FILES=$(shell find tests/python -name "[^_]*.py")
PYTHON_TESTS=$(patsubst %,test-%,$(PYTHON_TEST_FILES))

# Run all tests
tests:	tests-python


# Runs the python tests
tests-python:	$(PYTHON_TESTS)

$(PYTHON_TESTS): test-%: %
	@PYTHONPATH=$(PYTHONPATH):src/python \
		python $<
