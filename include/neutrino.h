//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _NEUTRINO_H
#define _NEUTRINO_H

#include "c/stdc.h"
#include "io/file.h"
#include "utils/clock.h"

// Settings to apply when creating a runtime. This struct gets passed by value
// under some circumstances so be sure it doesn't break anything to do that.
typedef struct {
  // The size in bytes of the space to create.
  size_t semispace_size_bytes;
  // The max amount of memory we'll allocate from the system. This is mainly a
  // failsafe in case a bug causes the runtime to allocate out of control, which
  // has happened, because the OS doesn't necessarily handle that very well.
  size_t system_memory_limit;
  // How often, on average, to simulate an allocation failure when fuzzing?
  size_t gc_fuzz_freq;
  // Random seed used to initialize the pseudo random generator used to
  // determine when to simulate a failure when fuzzing.
  size_t gc_fuzz_seed;
  // The plugins to install in runtimes created from this config.
  const void **plugins;
  size_t plugin_count;
  // The object that provides access to the file system. Null means use the
  // system default.
  file_system_t *file_system;
  // The object that provides (or pretends to provide) access to system time.
  real_time_clock_t *system_time;
  // The seed used for the pseudo-random number generator used within this
  // runtime. Note that this seed is not the only source of nondeterminism in
  // a runtime so running the same program twice with the same seed will not
  // necessarily give the same result.
  uint64_t random_seed;
} neu_runtime_config_t;

// Initializes the fields of this runtime config to the defaults. These defaults
// aren't necessarily appropriate for any particular use, they are just a set
// of well-defined values such that the config has at least been initialized
// with something. For any particular use you typically want to perform an
// additional initialization step appropriate for that use.
void neu_runtime_config_init_defaults(neu_runtime_config_t *config);

#endif // _NEUTRINO_H
