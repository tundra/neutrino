//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _NEUTRINO_HH
#define _NEUTRINO_HH

#include "c/stdc.h"
#include "utils/refcount.hh"
#include "c/stdvector.hh"

BEGIN_C_INCLUDES
#include "neutrino.h"
END_C_INCLUDES

struct runtime_t;

namespace neutrino {

// Forward declarations.
template <typename T> class Maybe;
class NativeService;

// Stuff that must be public for whatever reason but are really implementation
// details.
namespace internal {

// The information about a message that is available through a Maybe<T>. This is
// factored into its own type such that it can be shared between maybes without
// having to explicitly deal with ownership. For internal use only.
class MaybeMessage : public tclib::refcount_shared_t {
protected:
  virtual size_t instance_size() { return sizeof(*this); }

private:
  template <typename T> friend class neutrino::Maybe;

  // Create an error with the given message. The message is copied so it's safe
  // to delete it after this call.
  explicit MaybeMessage(const char *message);
  ~MaybeMessage();

  const char *message() { return message_; }

  // The message string. Owned by this object.
  char *message_;
};

} // namespace internal

// Settings to apply when creating a runtime. This struct gets passed by value
// under some circumstances so be sure it doesn't break anything to do that.
class RuntimeConfig : public neu_runtime_config_t {
public:
  RuntimeConfig();
};

// An option type that either holds some value or not, and if it doesn't it may
// have a message that indicates why it doesn't. You can also leave out the
// type parameter to indicate that the value returned is irrelevant.
template <typename T = void*>
class Maybe : public tclib::refcount_reference_t<internal::MaybeMessage> {
public:
  // Initialize an empty option which neither has a value nor a message
  // indicating why.
  Maybe()
    : tclib::refcount_reference_t<internal::MaybeMessage>()
    , has_value_(false) { }

  // Constructs an option with the given value.
  Maybe(T value)
    : tclib::refcount_reference_t<internal::MaybeMessage>()
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
  static Maybe<T> with_message(const char *message = NULL) { return Maybe<T>(*new (tclib::kDefaultAlloc) internal::MaybeMessage(message)); }

  // Returns an option with the given value, or if none is specified the default
  // value for the given type.
  static Maybe<T> with_value(T value = T()) { return Maybe<T>(value); }

private:
  // This constructor takes a reference rather than a pointer to ensure that
  // there are no ambiguities with the T constructor when T is a pointer and
  // you pass NULL.
  Maybe(internal::MaybeMessage &message)
    : tclib::refcount_reference_t<internal::MaybeMessage>(&message)
    , has_value_(false)
    , value_(NULL) { }

  bool has_value_;
  T value_;
};

// All the data associated with a single VM instance.
class Runtime {
public:
  // Abstract interface for objects that can be deleted.
  class Deletable;

  // Create but don't initialize this runtime.
  Runtime();

  // Dispose this runtime and all values owned by it.
  ~Runtime();

  // Initialize this runtime according to the given config.
  Maybe<> initialize(RuntimeConfig *config = NULL);

  // Add a native service to the set that will be installed when the runtime
  // is initialized. So native services must be added before initialize has been
  // called.
  void add_service(NativeService *service);

  // Has this runtime been successfully initialized?
  bool is_initialized() { return internal_ != NULL; }

  // Accessors for the underlying runtime.
  // TODO: remove.
  runtime_t *operator*();
  runtime_t *operator->();

private:
  // The Internal class is where most of the action is, it's hidden away in
  // the implementation to avoid exposing the mechanics of how runtimes work
  // internally.
  class Internal;
  friend class Internal;

  // Add a piece of data that should be cleaned up when this runtime is
  // destroyed.
  void take_ownership(Deletable *obj);

  // Underlying implementation.
  Internal *internal_;

  // Services to install on initialize.
  std::vector<NativeService*> services_;

  // Values this runtime has taken ownership of.
  std::vector<Deletable*> owned_;
};

} // namespace neutrino

#endif // _NEUTRINO_HH
