//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _NEUTRINO_REMOTE_HH
#define _NEUTRINO_REMOTE_HH

#include "c/stdc.h"
#include "neutrino.hh"
#include "utils/callback.hh"

namespace neutrino {

class ServiceRequest {
public:
  virtual ~ServiceRequest() { }
  virtual void fulfill(int64_t value) = 0;
};

class NativeServiceBinder {
public:
  typedef tclib::callback_t<void(ServiceRequest*)> MethodCallback;
  virtual ~NativeServiceBinder() { }
  virtual Maybe<> add_method(const char *name, MethodCallback callback) = 0;
};

class NativeService {
public:
  virtual ~NativeService() { }
  virtual Maybe<> install(NativeServiceBinder *config) = 0;
  virtual const char *name() = 0;
};

} // namespace neutrino

#endif // _NEUTRINO_REMOTE_HH
