//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "c/stdc.h"

#include "include/neutrino.hh"
#include "include/service.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "freeze.h"
#include "interp.h"
#include "plankton.h"
#include "runtime-inl.h"
#include "safe-inl.h"
#include "serialize.h"
#include "tagged.h"
#include "try-inl.h"
#include "utils/crash.h"
#include "utils/log.h"
#include "value.h"
END_C_INCLUDES

// Holds all the options understood by the main executable.
typedef struct {
  // The config to store config-related flags directly into.
  neu_runtime_config_t *config;
  // The reader that owns the parsed data.
  pton_command_line_reader_t *owner;
  // Parsed command-line.
  pton_command_line_t *cmdline;
} main_options_t;

// Initializes a options struct.
static void main_options_init(main_options_t *flags, neu_runtime_config_t *config) {
  flags->config = config;
  flags->owner = NULL;
  flags->cmdline = NULL;
}

// Free any memory allocated for the options struct.
static void main_options_dispose(main_options_t *flags) {
  pton_dispose_command_line_reader(flags->owner);
  flags->owner = NULL;
  flags->cmdline = NULL;
}

// Parse a set of command-line arguments.
static bool parse_options(int argc, const char **argv, main_options_t *flags_out) {
  pton_command_line_reader_t *reader = pton_new_command_line_reader();
  flags_out->owner = reader;
  pton_command_line_t *cmdline = pton_command_line_reader_parse(reader, argc, argv);
  flags_out->cmdline = cmdline;
  if (!pton_command_line_is_valid(cmdline)) {
    pton_syntax_error_t *error = pton_command_line_error(cmdline);
    ERROR("Error parsing command line options at char '%c'",
        pton_syntax_error_offender(error));
    return false;
  }

  flags_out->config->gc_fuzz_freq = (uint32_t) pton_int64_value(
      pton_command_line_option(cmdline,
          pton_c_str("garbage-collect-fuzz-frequency"),
          pton_integer(0)));
  flags_out->config->gc_fuzz_seed = (uint32_t) pton_int64_value(
      pton_command_line_option(cmdline,
          pton_c_str("garbage-collect-fuzz-seed"),
          pton_integer(0)));
  return true;
}

// Reads a library from the given library path and adds the modules to the
// runtime's module loader.
static value_t load_library_from_file(runtime_t *runtime, value_t self,
    pton_variant_t library_path) {
  // Read the library from the file.
  utf8_t library_path_str = new_c_string(pton_string_chars(library_path));
  TRY_DEF(library_path_val, new_heap_utf8(runtime, library_path_str));
  file_streams_t streams = file_system_open(runtime->file_system,
      library_path_str, OPEN_FILE_MODE_READ);
  if (!streams.is_open)
    return new_system_error_condition(seFileNotFound);
  TRY_FINALLY {
    E_RETURN(runtime_load_library_from_stream(runtime, streams.in, library_path_val));
  } FINALLY {
    file_streams_close(&streams);
  } YRT
}

// Constructs a module loader based on the given command-line options.
static value_t build_module_loader(runtime_t *runtime, pton_command_line_t *cmdline) {
  pton_variant_t options = pton_command_line_option(cmdline,
      pton_c_str("module_loader"), pton_null());
  if (pton_is_null(options))
    return success();
  value_t loader = deref(runtime->s_module_loader);
  pton_variant_t libraries = pton_map_get(options, pton_c_str("libraries"));
  for (uint32_t i = 0; i < pton_array_length(libraries); i++) {
    pton_variant_t library_path = pton_array_get(libraries, i);
    TRY(load_library_from_file(runtime, loader, library_path));
  }
  return success();
}

// Override some of the basic defaults to make the config better suited for
// running scripts.
static void runtime_config_init_main_defaults(neu_runtime_config_t *config) {
  // Currently the runtime doesn't handle allocation failures super well
  // (particularly plankton parsing) so keep the semispace size big.
  config->semispace_size_bytes = 10 * kMB;
}

// Native echo service, mainly for testing.
class EchoService : public neutrino::NativeService {
public:
  virtual neutrino::Maybe<> bind(neutrino::NativeServiceBinder *config);
  void echo(neutrino::ServiceRequest *request);
};

neutrino::Maybe<> EchoService::bind(neutrino::NativeServiceBinder *config) {
  config->set_namespace_name("echo");
  return config->add_method("echo", tclib::new_callback(&EchoService::echo, this));
}

void EchoService::echo(neutrino::ServiceRequest *request) {
  request->fulfill(request->argument(plankton::Variant::integer(0)));
}

// Create a vm and run the program under the given set of options.
static value_t neutrino_main_with_options(neutrino::RuntimeConfig *config,
    main_options_t *options) {
  neutrino::Runtime runtime;
  EchoService echo;
  runtime.add_service(&echo);
  runtime.initialize(config);
  size_t argc = 0;
  CREATE_SAFE_VALUE_POOL(*runtime, 4, pool);
  TRY_FINALLY {
    value_t result = whatever();
    E_TRY(build_module_loader(*runtime, options->cmdline));
    argc = pton_command_line_argument_count(options->cmdline);
    for (uint32_t i = 0; i < argc; i++) {
      pton_variant_t filename_var = pton_command_line_argument(options->cmdline, i);
      utf8_t filename = new_string(pton_string_chars(filename_var),
          pton_string_length(filename_var));
      value_t input;
      if (string_equals_cstr(filename, "-")) {
        in_stream_t *in = file_system_stdin(runtime->file_system);
        E_TRY_SET(input, read_stream_to_blob(*runtime, in));
      } else {
        file_streams_t streams = file_system_open(runtime->file_system, filename,
            OPEN_FILE_MODE_READ);
        if (!streams.is_open)
          return new_system_call_failed_condition("fopen");
        E_TRY_SET(input, read_stream_to_blob(*runtime, streams.in));
        file_streams_close(&streams);
      }
      E_TRY_DEF(program, safe_runtime_plankton_deserialize_blob(*runtime, protect(pool, input)));
      result = safe_runtime_execute_syntax(*runtime, protect(pool, program));
    }
    E_RETURN(result);
  } FINALLY {
    DISPOSE_SAFE_VALUE_POOL(pool);
  } YRT
}

// Set up the environment, parse arguments, create a vm, and run the program.
static value_t neutrino_main(int raw_argc, const char **argv) {
  neutrino::RuntimeConfig config;
  runtime_config_init_main_defaults(&config);
  // Set up a custom allocator we get tighter control over allocation.
  limited_allocator_t limited_allocator;
  limited_allocator_install(&limited_allocator, config.system_memory_limit);

  // Parse the options.
  main_options_t options;
  main_options_init(&options, &config);
  if (!parse_options(raw_argc - 1, argv + 1, &options))
    return new_invalid_input_condition();

  TRY_FINALLY {
    E_RETURN(neutrino_main_with_options(&config, &options));
  } FINALLY {
    main_options_dispose(&options);
    limited_allocator_uninstall(&limited_allocator);
  } YRT
}

int main(int argc, const char *argv[]) {
  install_crash_handler();
  value_t result = neutrino_main(argc, argv);
  if (is_condition(result)) {
    out_stream_t *out = file_system_stderr(file_system_native());
    print_ln(out, "Error: %v", result);
    return 1;
  } else {
    return 0;
  }
}
