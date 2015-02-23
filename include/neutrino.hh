//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _NEUTRINO_HH
#define _NEUTRINO_HH

#include "c/stdc.h"
#include "utils/refcount.hh"

BEGIN_C_INCLUDES
#include "neutrino.h"
END_C_INCLUDES

struct runtime_t;

namespace neutrino {

// Settings to apply when creating a runtime. This struct gets passed by value
// under some circumstances so be sure it doesn't break anything to do that.
class RuntimeConfig : public runtime_config_t {
public:
  RuntimeConfig();
};

// The information about an error that is available through a Maybe<T>. This is
// factored into its own type such that it can be shared between maybes without
// having to explicitly deal with ownership. For internal use only.
class MaybeError : public tclib::refcount_shared_t {
private:
  template <typename T> friend class Maybe;

  // Create an error with the given message. The message is copied so it's safe
  // to delete it after this call.
  explicit MaybeError(const char *message);
  ~MaybeError();

  const char *message() { return message_; }
  char *message_;
};

// An option type that either holds some value or not, and if it doesn't it may
// have a message that indicates why it doesn't.
template <typename T>
class Maybe : public tclib::refcount_reference_t<MaybeError> {
public:
  // Constructs an option with the given value.
  Maybe(T *value)
    : tclib::refcount_reference_t<MaybeError>()
    , has_value_(true)
    , value_(value) { }

  // A maybe evaluates true as a boolean if it represents a success, regardless
  // of the value it holds.
  operator bool() const { return has_value_; }

  // Does this option hold a value?
  bool has_value() const { return has_value_; }

  // Returns the message that indicates why this option doesn't have a value.
  // May return NULL if no message is available.
  const char *message() { return refcount_shared()->message(); }

  // Returns an option that has no value for the given reason, or optionally
  // for no explicit reason.
  static Maybe<T> with_message(const char *message = NULL) { return Maybe<T>(*new MaybeError(message)); }

  // Returns an option with the given value.
  static Maybe<T> with_value(T *value) { return Maybe<T>(value); }

private:
  Maybe(MaybeError &error)
    : tclib::refcount_reference_t<MaybeError>(&error)
    , has_value_(false)
    , value_(NULL) { }

  bool has_value_;
  T *value_;
};

// All the data associated with a single VM instance.
class Runtime {
public:
  Runtime();
  ~Runtime();

  // Initialize this runtime.
  Maybe<void> initialize(RuntimeConfig *config = NULL);

private:
  runtime_t *runtime_;
};

} // namespace neutrino

#endif // _NEUTRINO_HH
