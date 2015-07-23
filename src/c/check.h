//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Support for built-in methods.


#ifndef _CHECK
#define _CHECK

#include "sentry-inl.h"
#include "utils/check.h"

// Shorthands for sentry checks
#define CHECK_DOMAIN(vdDomain, EXPR) CHECK_SENTRY(snInDomain(vdDomain), EXPR)
#define CHECK_DOMAIN_OPT(vdDomain, EXPR) CHECK_SENTRY(snInDomainOpt(vdDomain), EXPR)
#define CHECK_FAMILY(ofFamily, EXPR) CHECK_SENTRY(snInFamily(ofFamily), EXPR)
#define CHECK_FAMILY_OPT(ofFamily, EXPR) CHECK_SENTRY(snInFamilyOpt(ofFamily), EXPR)
#define CHECK_FAMILY_OR_NULL(ofFamily, EXPR) CHECK_SENTRY(snInFamilyOrNull(ofFamily), EXPR)
#define CHECK_PHYLUM(tpPhylum, EXPR) CHECK_SENTRY(snInPhylum(tpPhylum), EXPR)
#define CHECK_PHYLUM_OPT(tpPhylum, EXPR) CHECK_SENTRY(snInPhylumOpt(tpPhylum), EXPR)

#define EXPECT_FAMILY(ofFamily, EXPR) EXPECT_SENTRY(snInFamily(ofFamily), EXPR)

#endif // _CHECK
