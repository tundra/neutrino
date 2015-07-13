//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "sync.h"
#include "undertaking.h"

void undertaking_init(undertaking_t *undertaking, undertaking_controller_t *controller) {
  undertaking->controller = controller;
  undertaking->state = usInitialized;
}

#define DEFINE_UNDERTAKING_CONTROLLER(Name, name, type_t)                      \
undertaking_controller_t k##Name##Controller = {                               \
  (undertaking_finish_f*) name##_undertaking_finish,                           \
  (undertaking_destroy_f*) name##_undertaking_destroy                          \
};
ENUM_UNDERTAKINGS(DEFINE_UNDERTAKING_CONTROLLER)
#undef DEFINE_UNDERTAKING_CONTROLLER
