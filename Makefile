# Run all tests
tests:	tests-python


# Runs the python tests
tests-python:
	@PYTHONPATH=$(PYTHONPATH):src/python \
		python tests/python/plankton/test_codec.py
