//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "neutrino.hh"
#include "plankton.hh"
#include "service.hh"
#include "utils/callback.hh"

BEGIN_C_INCLUDES
#include "plugin.h"
#include "runtime.h"
#include "utils/log.h"
#include "value-inl.h"
END_C_INCLUDES

using namespace neutrino;

// That "actual" implementation of a runtime.
class Runtime::Internal {
public:
  Internal(Runtime *owner);
  ~Internal();

  // The raw hook called from the runtime initialized.
  opaque_t service_install_hook_trampoline(opaque_t opaque_context);

  // Does the actual installation of services.
  value_t service_install_hook(service_install_hook_context_t *context);

  // Create and initialize this runtime.
  Maybe<> initialize(RuntimeConfig *config);

  // The runtime to which this internal belongs.
  Runtime *owner() { return owner_; }

  // The underlying live runtime.
  runtime_t *runtime() { return runtime_; }

private:
  Runtime *owner_;
  runtime_t *runtime_;
};

// TODO: factor this out into a tclib class, we need this elsewhere.
class Runtime::Deletable {
public:
  virtual ~Deletable() { }
  virtual size_t instance_size() = 0;
};

// Concrete implementation of the native service binder interface.
class NativeServiceBinderImpl
    : public NativeServiceBinder
    , public Runtime::Deletable {
public:
  // An adaptor that allows a native C++ method to be used from inside the
  // runtime.
  class MethodBridge {
  public:
    MethodBridge(NativeServiceBinderImpl *owner, plankton::Variant selector,
        MethodCallback direct);

    // Method that conforms to the runtime's native method hook that calls the
    // underlying C++ method.
    opaque_t invoke(opaque_t args);

    // Returns the adapted c-style callback. Must only be called after the
    // binder is done binding.
    unary_callback_t *bridge();

  public:
    NativeServiceBinderImpl *owner_;
    plankton::Variant selector_;
    NativeServiceBinder::MethodCallback original_;

    // The adapted callback we'll pass to the runtime. Note that because this
    // uses a direct pointer to the bridge, and the bridges are stored by value,
    // this can only be set after we're done manipulating the bridges array so
    // it's not set by the constructor.
    tclib::callback_t<opaque_t(opaque_t)> adapted_;
  };

  NativeServiceBinderImpl(plankton::Arena *scratch_arena,
      service_install_hook_context_t *context);

  // Implicitly cleans up the vectors.
  virtual ~NativeServiceBinderImpl() { }

  virtual Maybe<> add_method(plankton::Variant selector, MethodCallback callback);

  virtual void set_namespace_name(plankton::Variant value);

  virtual plankton::Factory *factory() { return scratch_arena_; }

  // Process the given service, modifying this binder in the process. If
  // successful a C-style descriptor will be stored in the out argument.
  value_t process(NativeService *service, service_descriptor_t **desc_out);

protected:
  virtual size_t instance_size() { return sizeof(*this); }

private:
  service_install_hook_context_t *context_;
  service_descriptor_t desc_;

  plankton::Variant namespace_name_;

  // Scratch arena that is made available to the service being bound. This
  // arena is only guaranteed to be available initially, until processing is
  // done, so to be on the safe side it's cleared at that point.
  plankton::Arena *scratch_arena_;

  // This binder's method bridges. This must not be modified when is_frozen_ is
  // true.
  std::vector<MethodBridge> bridges_;

  // Storage for the method array we'll pass to the C runtime.
  std::vector<service_method_t> methods_;

  // Set to true when no further modification is allowed.
  bool is_frozen_;
};

// Concrete implementation of the service request interface.
class ServiceRequestImpl : public ServiceRequest {
public:
  ServiceRequestImpl(native_request_t *native) : native_(native) { }
  virtual void fulfill(plankton::Variant result);
  virtual plankton::Factory *factory();
private:
  native_request_t *native_;
};

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
  for (size_t i = 0; i < owned_.size(); i++)
    delete owned_[i];
  owned_.clear();
}

void Runtime::add_service(NativeService *service) {
  CHECK_FALSE("adding service after initialized", is_initialized());
  services_.push_back(service);
}

void Runtime::take_ownership(Deletable *obj) {
  owned_.push_back(obj);
}

runtime_t *Runtime::operator*() {
  return (internal_ == NULL) ? NULL : internal_->runtime();
}

runtime_t *Runtime::operator->() {
  return this->operator*();
}

Maybe<> Runtime::initialize(RuntimeConfig *config) {
  if (internal_ != NULL)
    return Maybe<>::with_message("Runtime has already been initialized");
  internal_ = new (tclib::kDefaultAlloc) Internal(this);
  return internal_->initialize(config);
}

Runtime::Internal::Internal(Runtime *owner)
  : owner_(owner)
  , runtime_(NULL) { }

Runtime::Internal::~Internal() {
  if (runtime_ != NULL) {
    delete_runtime(runtime_);
    runtime_ = NULL;
  }
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
    plankton::Arena scratch_arena;
    NativeServiceBinderImpl *binder = new NativeServiceBinderImpl(&scratch_arena,
        context);
    // Tie the binder's lifetime to the runtime as a whole.
    owner_->take_ownership(binder);
    service_descriptor_t *desc = NULL;
    TRY(binder->process(service, &desc));
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

NativeServiceBinderImpl::MethodBridge::MethodBridge(NativeServiceBinderImpl *owner,
    plankton::Variant selector, MethodCallback original)
  : owner_(owner)
  , selector_(selector)
  , original_(original) { }

unary_callback_t *NativeServiceBinderImpl::MethodBridge::bridge() {
  CHECK_TRUE("binder must be frozen", owner_->is_frozen_);
  if (adapted_.is_empty())
    adapted_ = tclib::new_callback(&MethodBridge::invoke, this);
  return unary_callback_from(&adapted_);
}

opaque_t NativeServiceBinderImpl::MethodBridge::invoke(opaque_t args) {
  ServiceRequestImpl request(static_cast<native_request_t*>(o2p(args)));
  original_(&request);
  return opaque_null();
}

NativeServiceBinderImpl::NativeServiceBinderImpl(plankton::Arena *scratch_arena,
    service_install_hook_context_t *context)
  : context_(context)
  , scratch_arena_(scratch_arena)
  , is_frozen_(false) { }

Maybe<> NativeServiceBinderImpl::add_method(plankton::Variant selector,
    NativeServiceBinder::MethodCallback callback) {
  CHECK_FALSE("modifying frozen", is_frozen_);
  MethodBridge method(this, selector, callback);
  bridges_.push_back(method);
  return Maybe<>::with_value();
}

void NativeServiceBinderImpl::set_namespace_name(plankton::Variant value) {
  namespace_name_ = value;
}

value_t NativeServiceBinderImpl::process(NativeService *service,
    service_descriptor_t **desc_out) {
  service->bind(this);
  is_frozen_ = true;
  for (size_t i = 0; i < bridges_.size(); i++) {
    MethodBridge *bridge = &bridges_[i];
    service_method_t method_out = {bridge->selector_.to_c(), bridge->bridge()};
    methods_.push_back(method_out);
  }
  service_descriptor_init(&desc_, namespace_name_.to_c(), namespace_name_.to_c(),
      methods_.size(), methods_.data());
  *desc_out = &desc_;
  // This arena is not guaranteed to be valid once this call returns to clear
  // it just in case.
  scratch_arena_ = NULL;
  return success();
}

plankton::Factory *ServiceRequestImpl::factory() {
  if (native_->arena == NULL)
    native_->arena = pton_new_arena();
  return static_cast<plankton::Arena*>(native_->arena);
}

void ServiceRequestImpl::fulfill(plankton::Variant result) {
  pton_variant_t as_c = result.to_c();
  native_request_fulfill(native_, &as_c);
}

internal::MaybeMessage::MaybeMessage(const char *message)
  : message_((message == NULL) ? NULL : strdup(message)) { }

internal::MaybeMessage::~MaybeMessage() {
  free(message_);
  message_ = NULL;
}
