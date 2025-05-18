#pragma once

#include "text.h"

struct options {
  struct string hostname;
  struct string port;

  struct string videoId;
};

enum options_error {
  OPTIONS_ERROR_NONE,
  OPTIONS_ERROR_INSTANCE_REQUIRED,
  OPTIONS_ERROR_INSTANCE_INVALID,
  OPTIONS_ERROR_VIDEO_REQUIRED,
  OPTIONS_ERROR_VIDEO_INVALID,
  OPTIONS_ERROR_HELP,
};

internalfn void
OptionsInit(struct options *options);

internalfn enum options_error
OptionsParse(struct options *options, u32 argumentCount, char **arguments);
