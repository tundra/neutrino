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
  value_t value = new_runtime(config, &runtime_);
  Maybe<void> result;
  if (is_condition(value)) {
    value_to_string_t to_string;
    value_to_string(&to_string, value);
    result = Maybe<void>::with_message(to_string.str.chars);
    value_to_string_dispose(&to_string);
  } else {
    result = Maybe<void>::with_value(NULL);
  }
  return result;
}

MaybeMessage::MaybeMessage(const char *message)
  : message_((message == NULL) ? NULL : strdup(message)) { }

MaybeMessage::~MaybeMessage() {
  free(message_);
  message_ = NULL;
}
