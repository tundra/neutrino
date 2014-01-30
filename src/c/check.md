Runtime checks
==============

The neutrino runtime and tests uses a handful of different kinds of checks to indicate errors. The structure is like this:

 * A `CHECK` is a runtime assertion. If a check fails it means that there's an error in the logic of the runtime and it's not safe to continue so the vm should abort. Checks can be disabled so they must not have side-effects. All checks take a message argument as the first argument which must be a literal string and which is printed when the check fails.
 * An `EXPECT` is a "soft" runtime assertion that does not indicate a logic error but for instance unexpected input. It bails out with a condition which can be handled rather than abort and can not be disabled and hence is allowed to have side-effects.
 * A `COND_CHECK` is like a check except that for testing the `abort` function may be disabled and a condition returned instead so that it's possible to test whether the check is triggered.
 * An `ASSERT` is like a check but used only by tests (they're not visible within the runtime). The can't be disabled so it's fine, and indeed very common, for them to have side-effects.
 
## Testing

It is important that `CHECK`s operate correctly and catch the problems they're intended to catch and so it's important that they can be tested. For this reason, if it is safe for code to continue after a `CHECK` has failed it should use a `COND_CHECK` and continue in a well-defined way, documenting what happens if a `CHECK` fails. For instance, if an index is out of bounds the method may still return a well-defined value. This way tests can override the `abort` function and rely on the well-defined check failure behavior to test the check failure case. In some cases there may be no way to continue and there it's fine to let the runtime crash without documenting it, since tests should only rely on well-defined check failure behavior if it is explicitly documented.
