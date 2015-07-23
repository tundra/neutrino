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
#define CHECK_SYNTAX_FAMILY_OPT(EXPR) CHECK_SENTRY(snInSyntaxFamilyOpt, EXPR)

#define CHECK_PHYLUM(tpPhylum, EXPR) CHECK_SENTRY(snInPhylum(tpPhylum), EXPR)
#define CHECK_PHYLUM_OPT(tpPhylum, EXPR) CHECK_SENTRY(snInPhylumOpt(tpPhylum), EXPR)

#define CHECK_GENUS(dgGenus, EXPR) CHECK_SENTRY(snInGenus(dgGenus), EXPR)
#define CHECK_GENUS_OPT(dgGenus, EXPR) CHECK_SENTRY(snInGenusOpt(dgGenus), EXPR)

#define EXPECT_FAMILY(ofFamily, EXPR) EXPECT_SENTRY(snInFamily(ofFamily), EXPR)

// Check that fails unless the given expression is in a mutable mode.
#define CHECK_MUTABLE(EXPR)                                                    \
IF_CHECKS_ENABLED(CHECK_TRUE("mutable", is_mutable(EXPR)))

// Check that fails unless the given expression is deep frozen.
#define CHECK_DEEP_FROZEN(EXPR)                                                \
IF_CHECKS_ENABLED(CHECK_TRUE("deep frozen", peek_deep_frozen(EXPR)))

// Check that fails unless the given expression is frozen.
#define CHECK_FROZEN(EXPR)                                                     \
IF_CHECKS_ENABLED(CHECK_TRUE("frozen", is_frozen(EXPR)))

// Check that fails unless the species is in the specified division.
#define CHECK_DIVISION(sdDivision, EXPR)                                       \
IF_CHECKS_ENABLED(__CHECK_CLASS__(species_division_t, sdDivision, EXPR, get_species_division))

#endif // _CHECK
