Neutrino implementation
=======================

At this phase in the implementation of neutrino the goal is to bootstrap the language. The sequence is roughly as follows:

 - Implement a basic interpreter in python and C.
 - Implement a translator from neutrino to C.
 - Gradually replace the python and C code with neutrino.

The runtime is implemented in C, the source parser in python. We need a data exchange format for sending data between communicating processes anyway, that format is called *plankton* and it is used for communicating between python, C, and neutrino during bootstrapping too.

Once there is a translation-based self-hosting system the next step will be to introduce a JIT and use it to generate executables directly, skipping the static translation step and C code completely. The ultimate goal is to self-host the language [Jikes](http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.151.5253)-style. The performance implications should be negligable or even positive (because there won't be a language barrier) and the benefits in ease of testing, debugging, and development will be massive.
