The binding process is somewhat elaborate (it doesn't help that it's written in C). This is an overview.

The entry-point to module binding is `build_bound_module` which takes a single root module and a module loader (indirectly through the runtime) and performs the complete binding process.

### Terminology

The input to the process are *unbound* modules. Being unbound means that all references outside the module are by name, there are no direct references to other modules. The output of the binding process is a *bound* module where all those named references have either been replaced by a direct reference to the thing they're naming, or the context of the reference will be able to provide the thing without further initialization.

Each module is divided into *fragments*, one for each stage. So if module `foo` has three stages, `$foo`, `@foo`, and `@@foo` there will be three fragments, one for `$`. `@`, and `@@`.

### Module enumeration

The first step is to take the input module (which is unbound) and recursively enumerate its imports. This is done by `build_transitive_module_array`. For each import the corresponding unbound module is obtained from the module loader and added to the list of modules. After this step the module loader is no longer used, all imports can be resolved directly from the set of modules built in this step.

### Building fragment entry map

The next step is to build an *entry* for each fragment; this is done by `build_fragment_entry_map`. The binding process happens fragment-by-fragment and the entries give the context you need to bind a fragment. The entries are built by scanning through the module list and building up a mapping from paths to fragment maps, a fragment map being itself a mapping from stages to fragment entries. So to get the entry for `@foo` you'd look up `foo` in the root mapping to get a fragment map for `foo` and then looking up `@` in the fragment map.

At this point we also create *synthetic fragments*; this is done by `build_synthetic_fragment_entries`. If module `$a` imports `$b`, where `b` has two fragments, `$b` and `@b`, it's convenient to create a `@a` fragment even if the `a` module doesn't have one initially. Synthetic and non-synthetic entries can be told apart because the field that holds the corresponding unbound fragment will be nothing.

In addition to creating the entries we calculate for each fragment which other fragments they import. So, for instance, if as above `$a` imports `$b` which also has `@b` we'll "unfold" the import and record explicitly in `$a` that it imports `$b` and in `@a` that it imports `@b`.

### Building the binding schedule

Once the fragment entry map is complete we create a list of all the fragments mentioned anywhere in the entry map and sort it lexicographically -- so '$a' comes before '$b' and '@a' comes before '$a'.

We then build the binding schedule by repeatedly scanning through the fragment list and checking for each entry if all its imports, which are listed explicitly, have been bound. This is done by `build_binding_schedule`. When we find one that can be bound it's added to the schedule and we start over again. The result is a schedule that lists the fragments in the order they should be bound and where ties are broken by lexical ordering. The list will contain synthetic fragments.

### Binding fragments

Once the schedule has been created we can start binding fragments in the scheduled order. This is done by `execute_binding_schedule`. This is where the bound modules are actually created, all the previous steps are done using only the unbound modules and fragments.

For each fragment the entries are applied to create the namespace and methodspace bindings for the fragment (this part is skipped for synthetic fragments obviously). The imports are taken from the fragment entry since it has had fragment imports resolved, and since that array is well-defined even for synthetic fragments that don't have a corresponding unbound fragment.

At the end of this step all fragments have been bound and we're done.
