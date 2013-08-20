## Method lookup

Method lookup is the operation of finding the unique method whose signature best matches an invocation. This is nontrivial because of keyword arguments.

Take this method declaration:

    def $this.foo(x:, y:, z:) => ...

Each parameter has two tags, then name and the positional offset so that the method can be called both with and without tags. The signature consists of seven tags, three parameter descriptors:

    {this: <this>, name: <name>, "x": <x>, 0: <x>, "y": <y>, 1: <y>, "z": <z>, 2: <z>}

Each parameter has a *parameter index* which matches the declaration, so the `<this>` parameter has index 0, `<name>` has index 1, etc. To make lookup more efficient the signature stores the parameters in order sorted by tag, so this record is stored as

    [(name, <name>), (this, <this>), (0, <x>), (1, <y>), (2, <z>), ("x", <x>), ("y", <y>), ("z", <z>)]

Each parameter object knows what its parameter index is so that information isn't lost by storing the parameters in a different order.

Now, take this invocation:

    $bar.foo(z: $bla, x: $abl, y: $bab)

Each argument has a tag and a value:

    {this: $bar, name: "foo", "z": $bla, "x": $abl, "y": $bab}

The structure and tags will always be the same and is stored in an *invocation record*.

    [this, name, "z", "x", "y"]

To match the signature the tags in an invocation record are stored in sorted order and the evaluation order is stored along with the tag. To add to the confusion though, the evaluation order is stored as *offset from the top of the stack* which gives the last argument to be evaluated index 0 and so forth. In this case, since the `"y"` parameter is evaluated last and `"this"` first the record contains:

    [(name, 3), (this, 4), ("x", 1), ("y", 0), ("z", 2)]

Because of all this there are five (!) ways to sort tags and it's important to always keep track of which you're using:

 * Parameter order. This is the order of method parameters as they're written in the syntax.
 * Sorted parameter order. This is the order of parameter tags after they've been sorted.
 * Evaluation order. This is the tags of an invocation in the order in which arguments are evaluated.
 * Stack offset order. This is the reverse of the evaluation order.
 * Sorted invocation order. This is the order of invocation tags after they've been sorted.

During lookup we're always dealing with the sorted orders. A lookup proceeds by a linear scan, matching the list of invocation tags, which are sorted, against the signature tags which are also sorted. For each match in the signature we check that the parameter hasn't been seen before (so if you pass arguments for tags `"x"` and `0` we'll recognize it) and record how well the argument matches the parameter.
