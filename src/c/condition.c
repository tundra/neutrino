//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "condition.h"

invalid_syntax_cause_t get_invalid_syntax_condition_cause(value_t condition) {
  return (invalid_syntax_cause_t) get_condition_details(condition);
}

const char *get_invalid_syntax_cause_name(invalid_syntax_cause_t cause) {
  switch (cause) {
#define __GEN_CASE__(Name) case is##Name: return #Name;
    ENUM_INVALID_SYNTAX_CAUSES(__GEN_CASE__)
#undef __GEN_CASE__
    default:
      return "unknown invalid syntax cause";
  }
}

const char *get_unsupported_behavior_cause_name(unsupported_behavior_cause_t cause) {
  switch (cause) {
#define __GEN_CASE__(Name) case ub##Name: return #Name;
    ENUM_UNSUPPORTED_BEHAVIOR_TYPES(__GEN_CASE__)
#undef __GEN_CASE__
    default:
      return "unknown unsupported behavior cause";
  }
}

const char *get_condition_cause_name(consition_cause_t cause) {
  switch (cause) {
#define __GEN_CASE__(Name) case cc##Name: return #Name;
    ENUM_CONDITION_CAUSES(__GEN_CASE__)
#undef __GEN_CASE__
    default:
      return "invalid condition";
  }
}

const char *get_lookup_error_cause_name(lookup_error_cause_t cause) {
  switch (cause) {
#define __GEN_CASE__(Name) case lc##Name: return #Name;
    ENUM_LOOKUP_ERROR_CAUSES(__GEN_CASE__)
#undef __GEN_CASE__
    default:
      return "unknown lookup error cause";
  }
}

const char *get_system_error_cause_name(system_error_cause_t cause) {
  switch (cause) {
#define __GEN_CASE__(Name) case se##Name: return #Name;
    ENUM_SYSTEM_ERROR_CAUSES(__GEN_CASE__)
#undef __GEN_CASE__
    default:
      return "unknown system error cause";
  }
}
