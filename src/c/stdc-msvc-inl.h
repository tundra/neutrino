//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include <limits>

// MSVC is missing some of the definitions from math.h so define them here.
// Also, the code gets compiled as C++ on windows so it's safe to use these
// sneaky C++ calls as long as there's no C++ in non-msvc specific code.

#ifndef INFINITY
#  define INFINITY std::numeric_limits<double>::infinity()
#endif

#ifndef NAN
#  define NAN std::numeric_limits<float>::quiet_NaN()
#endif

#define isfinite(V) _finite(V)

#define isnan(V) _isnan(V)
