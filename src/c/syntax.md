Syntax Trees
============

The runtime doesn't parse source files, it reads syntax trees directly as a binary object graph. The syntax tree nodes it understands are:

 * **Literal** A constant value. The value can be anything, a number, a string, an object, it doesn't matter.
 * **Array** An array constructor. Evaluates its subexpressions and packs their values into an array.
 * **Invocation** A multi-method invocation. Contains a list of tag/expression pairs, at runtime the expressions get evaluated and the {tag: value} record is matched against the multi-methods in the environment.
 * **Sequence** A sequence of expressions to evaluate; the result is the value of the last expression.

Syntax trees are converted to bytecode before they are executed.

Names, paths, identifiers
=========================

The "thing" used to refer to a definition, for instance `$a:b:c` or `@x:y:z`, has a particular anatomy and confusing the different parts can be confusing.

 * A full reference to a definition including the part that indicates the stage, for instance `$a:b:c`s or `@x:y:z`, is called an **identifier**. These are what you'll be used to from the surface language, where they're also called identifiers. At the identifier level there's a difference between `$a:b` and `@a:b` -- they refer to things in different stages.
 * If you strip off the stage indicator the remaining part, say `a:b:c`, is called a **path**. The two identifiers from before, `$a:b` and `@a:b`, are different because they belong to different stages but they have the same path component. A path has a head, the first part for instance `a` in `a:b:c`, and a tail, the rest for instance `b:c` in `a:b:c`. Lookup generally works by looking up the head and then recursively finding the tail in the result. All paths ultimately end with the empty path; trying to access the head or tail of the empty path is an error.
 * The segments of a path, for instance `a`, `b`, and `c` in `a:b:c`, are called **names**. Unlike identifiers and paths there is no special type for names, any value that supports transient identity can be used as a name, typically but not necessarily strings. These are the things you define in a toplevel definition. Confusingly, since paths support transient identity they can also be used as names. I'm not aware of any reason to do that though so you'll probably want to not do that.
 