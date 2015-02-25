//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "neutrino.hh"
#include "service.hh"
#include "utils/callback.hh"

BEGIN_C_INCLUDES
#include "plugin.h"
#include "runtime.h"
#include "utils/log.h"
#include "value-inl.h"
END_C_INCLUDES

using namespace neutrino;

class Runtime::Internal {
public:
  Internal(Runtime *owner);
  ~Internal();
  opaque_t service_install_hook_trampoline(opaque_t opaque_context);
  value_t service_install_hook(service_install_hook_context_t *context);
  Maybe<> initialize(RuntimeConfig *config);
  Runtime *owner() { return owner_; }
  runtime_t *runtime() { return runtime_; }
private:
  Runtime *owner_;
  runtime_t *runtime_;
};

class Runtime::Garbage {
public:
  virtual ~Garbage() { }
  virtual size_t instance_size() = 0;
};

Runtime::Internal::Internal(Runtime *owner)
  : owner_(owner)
  , runtime_(NULL) { }

Runtime::Internal::~Internal() {
  if (runtime_ != NULL) {
    delete_runtime(runtime_);
    runtime_ = NULL;
  }
}

RuntimeConfig::RuntimeConfig() {
  neu_runtime_config_init_defaults(this);
}

Runtime::Runtime()
  : internal_(NULL) { }

Runtime::~Runtime() {
  if (internal_ != NULL) {
    tclib::default_delete_concrete(internal_);
    internal_ = NULL;
  }
  for (size_t i = 0; i < garbage_.size(); i++)
    delete garbage_[i];
}

void Runtime::add_service(NativeService *service) {
  services_.push_back(service);
}

void Runtime::add_garbage(Garbage *garbage) {
  garbage_.push_back(garbage);
}

runtime_t *Runtime::operator*() {
  return (internal_ == NULL) ? NULL : internal_->runtime();
}

runtime_t *Runtime::operator->() {
  return (internal_ == NULL) ? NULL : internal_->runtime();
}

class NativeServiceBinderImpl : public NativeServiceBinder, public Runtime::Garbage {
public:
  class MethodBridge {
  public:
    MethodBridge(const char *name, NativeServiceBinder::MethodCallback direct)
      : name_(name)
      , direct_(direct) { }
    opaque_t call(opaque_t args);
  public:
    const char *name_;
    NativeServiceBinder::MethodCallback direct_;
    tclib::callback_t<opaque_t(opaque_t)> adapted_;
  };

  virtual ~NativeServiceBinderImpl() { }
  virtual size_t instance_size() { return sizeof(*this); }
  NativeServiceBinderImpl(service_install_hook_context_t *context);
  virtual Maybe<> add_method(const char *name, NativeServiceBinder::MethodCallback callback);
  value_t apply(NativeService *service, service_descriptor_t **desc_out);
private:
  service_install_hook_context_t *context_;
  service_descriptor_t desc_;
  std::vector<MethodBridge> bridges_;
  std::vector<service_method_t> methods_;
};

class ServiceRequestImpl : public ServiceRequest {
public:
  ServiceRequestImpl(native_request_t *native) : native_(native) { }
  virtual void fulfill(int64_t value);
private:
  native_request_t *native_;
};

opaque_t NativeServiceBinderImpl::MethodBridge::call(opaque_t args) {
  ServiceRequestImpl request(static_cast<native_request_t*>(o2p(args)));
  direct_(&request);
  return opaque_null();
}

void ServiceRequestImpl::fulfill(int64_t value) {
  native_request_fulfill(native_, new_integer(value));
}

NativeServiceBinderImpl::NativeServiceBinderImpl(service_install_hook_context_t *context)
  : context_(context) { }

Maybe<> NativeServiceBinderImpl::add_method(const char *name,
    NativeServiceBinder::MethodCallback callback) {
  MethodBridge method(name, callback);
  bridges_.push_back(method);
  return Maybe<>::with_value();
}

value_t NativeServiceBinderImpl::apply(NativeService *service,
    service_descriptor_t **desc_out) {
  desc_.name = service->name();
  service->install(this);
  desc_.methodc = bridges_.size();
  for (size_t i = 0; i < bridges_.size(); i++) {
    MethodBridge *bridge = &bridges_[i];
    bridge->adapted_ = tclib::new_callback(&MethodBridge::call, bridge);
    service_method_t method_out = {bridge->name_, unary_callback_from(&bridge->adapted_)};
    methods_.push_back(method_out);
  }
  desc_.methodv = methods_.data();
  *desc_out = &desc_;
  return success();
}

opaque_t Runtime::Internal::service_install_hook_trampoline(opaque_t opaque_context) {
  service_install_hook_context_t *context =
      static_cast<service_install_hook_context_t*>(o2p(opaque_context));
  return v2o(service_install_hook(context));
}

value_t Runtime::Internal::service_install_hook(service_install_hook_context_t *context) {
  std::vector<NativeService*> &services = owner()->services_;
  for (size_t i = 0; i < services.size(); i++) {
    NativeService *service = services[i];
    NativeServiceBinderImpl *binder = new NativeServiceBinderImpl(context);
    owner_->add_garbage(binder);
    service_descriptor_t *desc = NULL;
    TRY(binder->apply(service, &desc));
    TRY(service_hook_add_service(context, desc));
  }
  return success();
}

Maybe<> Runtime::Internal::initialize(RuntimeConfig *config) {
  extended_runtime_config_t extcfg = *extended_runtime_config_get_default();
  if (config != NULL)
    extcfg.base = *config;
  tclib::callback_t<opaque_t(opaque_t)> install_hook = tclib::new_callback(
      &Runtime::Internal::service_install_hook_trampoline, this);
  extcfg.service_install_hook = tclib::unary_callback_from(&install_hook);
  value_t value = new_runtime(&extcfg, &runtime_);
  Maybe<> result;
  if (is_condition(value)) {
    value_to_string_t to_string;
    value_to_string(&to_string, value);
    result = Maybe<>::with_message(to_string.str.chars);
    value_to_string_dispose(&to_string);
  } else {
    result = Maybe<>::with_value();
  }
  return result;
}

Maybe<> Runtime::initialize(RuntimeConfig *config) {
  if (internal_ != NULL)
    return Maybe<>::with_message("Runtime has already been initialized");
  internal_ = new (tclib::kDefaultAlloc) Internal(this);
  return internal_->initialize(config);
}

MaybeMessage::MaybeMessage(const char *message)
  : message_((message == NULL) ? NULL : strdup(message)) { }

MaybeMessage::~MaybeMessage() {
  free(message_);
  message_ = NULL;
}
