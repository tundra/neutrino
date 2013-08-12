## Process model

The execution state of a process is stored in a segmented stack and some helper data structures. Execution is "linear", meaning that you always return exactly once from a call. Neutrino execution supports these mechanisms:

 * Call/return with tail call elimination. The caller's frame is only left on the stack if the caller genuinely needs to be able to continue executing after the call returns.
 * One-shot escapes, where implicit return counts as a "shot". You can at any point grab a one-shot escape which gives you a function that can be called from a deeper call and which causes the stack to be unwound to the point where the escape was created. Escaping multiple times causes an error, as does calling the escape from a different process.
 * Unwind-protect, where a block of code is executed when its scope exits whether it exits via an implicit return or an escape being called.

The implementation works as follows. Each process has a stack which consists of a number of _stack pieces_. A stack piece is a relatively small contiguous block of memory, possibly with a pointer to a previous stack piece. At the beginning of a call a range of that block is reserved for the call's local state, that's a _frame_. We know how much local state a call may need since it's recorded in the code object being called. If the frame doesn't fit within what's left in the current piece a new piece is allocated and the frame is allocated there. Within the call the frame can be used as a normal stack would, no difference. A stack frame is allocated starting from the first field in the stack that hasn't been used by the caller, _not_ from the first field that hasn't been reserved. Often much of a frame will be unused so going from the top of the frame would be wasteful.

Unwind-protect creates a new stack frame which executes the code when you return to it, that deals with implicit returns, and pushes a pointer to the frame onto a chain of unwind-protect frames that it stored in the process separate from the stack. Executing the code also pops the pointer off the unwind-protect stack.

Grabbing a one-shot escape works by creating a new object with a pointer to the current process, a flag that indicates whether the escape has been fired, and a pointer to an unwind-protect frame which is also set up. The unwind-protect flips whether the escape has been fired. When returning implicitly the unwind-protect marks the object as fired so it can't be fired again. When fired explicitly we first check if it is being called from the escape's process and that it hasn't already been fired. If not it marks itself as fired and pops unwind-protect pointers off the unwind-protect stack and executes them until it reaches its own which it pops off without executing.

## Method lookup

The runtime structure that represents a method has two parts: a signature which is used to perform lookups and a code block which holds the method implementation. A signature contains the same information as the source method signature but in a more digested form. If has the following fields:

 * An array of all the method tags. For instance, the signature `def foo(x:, y:)` matches the explicit tags `"x"` and `"y"` and the implicit tags `this`, `name`, `0` and `1`. That makes the tag array `[name, this, 0, 1, "x", "y"]`.
 * An array of parameter descriptors with an entry for each entry in the tag array. In the above signature the `"x"` and `0` tags identify the same parameter, as does `"y"` and `1`. Hence entry 2 and 4 in the descriptor array will point to a descriptor for the `"x"`/`0` parameter, and 3 and 5 `"y"`/`1`. A descriptor holds any guards, the index of the parameter in the evaluation order, whether it is optional, etc.
 * The total number of parameters. If more arguments are given than this value the signature cannot match.
 * The total number of required parameters. If fewer arguments are given than this value the signature cannot match.
 * Whether all arguments must match a parameter or if extras are allowed.