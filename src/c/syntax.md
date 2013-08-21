Syntax Trees
============

The runtime doesn't parse source files, it reads syntax trees directly as a binary object graph. The syntax tree nodes it understands are:

 * **Literal** A constant value. The value can be anything, a number, a string, an object, it doesn't matter.
 * **Array** An array constructor. Evaluates its subexpressions and packs their values into an array.
 * **Invocation** A multi-method invocation. Contains a list of tag/expression pairs, at runtime the expressions get evaluated and the {tag: value} record is matched against the multi-methods in the environment.
 * **Sequence** A sequence of expressions to evaluate; the result is the value of the last expression.

Syntax trees are converted to bytecode before they are executed.
