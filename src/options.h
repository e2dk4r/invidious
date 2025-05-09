#pragma once

#include "text.h"

struct options {
  struct string hostname;
  struct string port;

  struct string videoId;
};

internalfn b8
OptionsParse(struct options *options, u32 argumentCount, char **arguments);
