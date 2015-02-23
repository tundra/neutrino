//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "neutrino.hh"

BEGIN_C_INCLUDES
#include "runtime.h"
#include "value-inl.h"
END_C_INCLUDES

using namespace neutrino;

RuntimeConfig::RuntimeConfig() {
  runtime_config_init_defaults(this);
}

Runtime::Runtime()
  : runtime_(NULL) { }

Runtime::~Runtime() {
  if (runtime_ != NULL) {
    delete_runtime(runtime_);
    runtime_ = NULL;
  }
}

Maybe<void> Runtime::initialize(RuntimeConfig *config) {
  value_t result = new_runtime(config, &runtime_);
  if (is_condition(result)) {
    value_to_string_t to_string;
    value_to_string(&to_string, result);
    Maybe<void> error = Maybe<void>::with_message(to_string.str.chars);
    value_to_string_dispose(&to_string);
    return error;
  } else {
    return NULL;
  }
}

MaybeError::MaybeError(const char *message)
  : message_((message == NULL) ? NULL : strdup(message)) { }

MaybeError::~MaybeError() {
  free(message_);
  message_ = NULL;
}
