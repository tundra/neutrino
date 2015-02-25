//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _NEUTRINO_SERVICE_HH
#define _NEUTRINO_SERVICE_HH

#include "c/stdc.h"
#include "neutrino.hh"
#include "plankton-inl.hh"
#include "utils/callback.hh"

namespace neutrino {

// A request set to a native service.
class ServiceRequest {
public:
  virtual ~ServiceRequest() { }

  // Schedules the result of the request to be fulfilled. The given variant must
  // either have been allocated using the factory provided by this request or
  // be known to be valid until this request's factory is destroyed. If it's
  // allocated elsewhere one way to ensure this is to give the request's factory
  // ownership of the factory using which it's allocated.
  virtual void fulfill(plankton::Variant result) = 0;

  // Returns a factory that can be used to allocate the result.
  virtual plankton::Factory *factory() = 0;
};

// A binder is used to describe a native service to the runtime.
//
// All variants passed into this binder must either be allocated within the
// factory supplied by the binder or must be alive at least until the initialize
// call returns that caused the bind call that supplied this binder.
class NativeServiceBinder {
public:
  // The type of functions that will be called to respond to requests.
  typedef tclib::callback_t<void(ServiceRequest*)> MethodCallback;
  virtual ~NativeServiceBinder() { }

  // Add a method with the given selector to the set understood by the service
  // being bound.
  virtual Maybe<> add_method(plankton::Variant selector, MethodCallback callback) = 0;

  // Set the name under which to bind the service. If no display name has been
  // set this also sets that.
  virtual void set_namespace_name(plankton::Variant name) = 0;

  // Sets the name to display when printing the service.
  virtual void set_display_name(plankton::Variant name) = 0;

  // Returns a factory that can be used to allocate variants used in the
  // definition of this service. The factory is only guaranteed to be valid
  // during the bind call to which this binder is passed.
  virtual plankton::Factory *factory() = 0;
};

// Abstract interface for native services.
class NativeService {
public:
  virtual ~NativeService() { }

  // Called during runtime initialization to configure the runtime's view of
  // this service. Note that this may be called any number of time, even for
  // the same runtime, with different binders so it must be idempotent except
  // for side-effects to the binder.
  virtual Maybe<> bind(NativeServiceBinder *config) = 0;
};

} // namespace neutrino

#endif // _NEUTRINO_SERVICE_HH
