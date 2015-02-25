//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _NEUTRINO_SERVICE_HH
#define _NEUTRINO_SERVICE_HH

#include "c/stdc.h"
#include "neutrino.hh"
#include "utils/callback.hh"

namespace neutrino {

// A request set to a native service.
class ServiceRequest {
public:
  virtual ~ServiceRequest() { }

  // Schedules the result of the request to be fulfilled.
  // TODO: accept arbitrary variants.
  virtual void fulfill(int64_t value) = 0;
};

// A binder is used to describe a native service to the runtime.
class NativeServiceBinder {
public:
  // The type of functions that will be called to respond to requests.
  typedef tclib::callback_t<void(ServiceRequest*)> MethodCallback;
  virtual ~NativeServiceBinder() { }

  // Add a method with the given name to the set understood by the service being
  // bound.
  virtual Maybe<> add_method(const char *name, MethodCallback callback) = 0;

  // Set the name under which to bind the service.
  // TODO: support arbitrary variants.
  virtual void set_namespace_name(const char *name) = 0;
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
