//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "format.h"
#include "tagged.h"
#include "utils.h"
#include "utils/ook-inl.h"
#include "utils/strbuf.h"
#include "value.h"

static void format_value(format_handler_o *self, format_request_t *request,
    va_list_ref_t argp) {
  // Format a value. Since value_t and encoded_value_t should be
  // synonymous at the native level it should be possible to pass values
  // directly. However if that ever fails just grab the encoded value
  // where the value is passed.
  encoded_value_t encoded = va_arg(VA_LIST_DEREF(argp), encoded_value_t);
  value_t value;
  value.encoded = encoded;
  size_t depth = (request->width == -1) ? kDefaultPrintDepth : ((size_t) request->width);
  uint32_t flags = pfNone;
  if ((request->flags & ffDash) != 0)
    flags = flags | pfCanonical;
  if ((request->flags & ffHash) != 0)
    flags = flags | pfUnquote;
  print_on_context_t context;
  print_on_context_init(&context, request->buf, flags, depth);
  value_print_on(value, &context);
}

IMPLEMENTATION(value_format_handler_o, format_handler_o);
VTABLE(value_format_handler_o, format_handler_o) { format_value };
format_handler_o kValueFormatHandler;

static void format_value_pointer(format_handler_o *self, format_request_t *request,
    va_list_ref_t argp) {
  encoded_value_t encoded = va_arg(VA_LIST_DEREF(argp), encoded_value_t);
  value_t decoded;
  decoded.encoded = encoded;
  if (is_same_value(decoded, new_rogue_sentinel(rsCanonicalized))) {
    // Print the canonicalized marker specially so that it's recognizable.
    string_buffer_native_printf(request->buf, "(canonicalized)");
  } else {
    char wordy[kMaxWordyNameSize];
    wordy_encode(encoded, wordy, kMaxWordyNameSize);
    string_buffer_native_printf(request->buf, "%s", wordy);
  }
}

IMPLEMENTATION(value_pointer_format_handler_o, format_handler_o);
VTABLE(value_pointer_format_handler_o, format_handler_o) { format_value_pointer };
format_handler_o kValuePointerFormatHandler;

void ensure_neutrino_format_handlers_registered() {
  static bool has_run = false;
  if (has_run)
    return;
  has_run = true;
  VTABLE_INIT(value_format_handler_o, &kValueFormatHandler);
  register_format_handler('v', &kValueFormatHandler);
  VTABLE_INIT(value_pointer_format_handler_o, &kValuePointerFormatHandler);
  register_format_handler('w', &kValuePointerFormatHandler);
}
