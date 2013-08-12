## Method lookup

The runtime structure that represents a method has two parts: a signature which is used to perform lookups and a code block which holds the method implementation. A signature contains the same information as the source method signature but in a more digested form. If has the following fields:

 * An array of all the method tags. For instance, the signature `def foo(x:, y:)` matches the explicit tags `"x"` and `"y"` and the implicit tags `this`, `name`, `0` and `1`. That makes the tag array `[name, this, 0, 1, "x", "y"]`.
 * An array of parameter descriptors with an entry for each entry in the tag array. In the above signature the `"x"` and `0` tags identify the same parameter, as does `"y"` and `1`. Hence entry 2 and 4 in the descriptor array will point to a descriptor for the `"x"`/`0` parameter, and 3 and 5 `"y"`/`1`. A descriptor holds any guards, the index of the parameter in the evaluation order, whether it is optional, etc.
 * The total number of parameters. If more arguments are given than this value the signature cannot match.
 * The total number of required parameters. If fewer arguments are given than this value the signature cannot match.
 * Whether all arguments must match a parameter or if extras are allowed.
